#include "BackgroundSlicingProcess.hpp"
#include "GUI_App.hpp"
#include "GUI.hpp"
#include "MainFrame.hpp"
#include "format.hpp"

#include <wx/app.h>
#include <wx/panel.h>
#include <wx/stdpaths.h>

// For zipped archive creation
#include <wx/stdstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <miniz.h>

// Print now includes tbb, and tbb includes Windows. This breaks compilation of wxWidgets if included before wx.
#include "libslic3r/Print.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/GCode/PostProcessor.hpp"
#include "libslic3r/GCode/Thumbnails.hpp"
#include "libslic3r/Format/SL1.hpp"
#include "libslic3r/Format/FormatConfig.hpp"
#include "libslic3r/Format/UFPWriter.hpp"
#include "libslic3r/Thread.hpp"
#include "libslic3r/libslic3r.h"

#include <cassert>
#include <stdexcept>
#include <cctype>
#include <sstream>
#include <iomanip>

#include <boost/format/format_fwd.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/algorithm/string.hpp>
#include "I18N.hpp"
//#include "RemovableDriveManager.hpp"

#include "slic3r/GUI/Plater.hpp"

// Generate a consistent GUID from material type for fallback
// This creates a deterministic UUID v5-like hash from the material name
static std::string generate_material_guid(const std::string& material_type) {
    if (material_type.empty()) {
        return "00000000-0000-0000-0000-000000000000";
    }
    
    // Simple hash function to generate a deterministic GUID
    // Using a basic string hash to create consistent UUIDs for the same material
    std::hash<std::string> hasher;
    size_t hash = hasher(material_type);
    
    // Format as UUID v4-like string (using hash for first 32 bits)
    // Use lowercase hex to match UltiMaker format
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::nouppercase;
    oss << std::setw(8) << (hash & 0xFFFFFFFF) << "-";
    oss << std::setw(4) << ((hash >> 32) & 0xFFFF) << "-";
    oss << std::setw(4) << (0x4000 | ((hash >> 48) & 0x0FFF)) << "-";  // Version 4 variant
    oss << std::setw(4) << (0x8000 | ((hash >> 16) & 0x3FFF)) << "-";   // RFC 4122 variant
    oss << std::setw(12) << ((hash & 0xFFFFFFFFFFFF) ^ 0x123456789ABC);
    
    return oss.str();
}

// Helper function to extract MATERIAL_GUID from filament preset
// Walks the inheritance chain to find the base preset that has the MATERIAL_GUID
// Also checks the selected preset itself first (in case it's a user-created preset with its own GUID)
static std::string extract_material_guid_from_preset(const Slic3r::Preset* filament_preset, const Slic3r::PresetBundle* preset_bundle, const std::string& preset_name_for_debug) {
    BOOST_LOG_TRIVIAL(warning) << "=== extract_material_guid_from_preset START ===";
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Searching for preset: '" << preset_name_for_debug << "'";
    
    if (!preset_bundle) {
        BOOST_LOG_TRIVIAL(error) << "extract_material_guid_from_preset: FAILURE - preset_bundle=NULL";
        return "";
    }
    
    // If filament_preset is NULL, try to find it using the preset_name_for_debug or other strategies
    const Slic3r::Preset* preset_to_use = filament_preset;
    if (!preset_to_use && !preset_name_for_debug.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: filament_preset is NULL, trying to find preset by name: '" << preset_name_for_debug << "'";
        preset_to_use = preset_bundle->filaments.find_preset(preset_name_for_debug, false);
        if (!preset_to_use) {
            // Try partial name matching
            std::string search_lower = preset_name_for_debug;
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
            for (const auto& preset : preset_bundle->filaments) {
                if (preset.is_system && preset.is_visible) {
                    std::string name_lower = preset.name;
                    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                    if (name_lower.find(search_lower) != std::string::npos) {
                        preset_to_use = &preset;
                        BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Found preset by partial name match: " << preset.name;
                        break;
                    }
                }
            }
        }
        
        if (!preset_to_use) {
            BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Could not find preset '" << preset_name_for_debug << "', will use fallback search";
        }
    }
    
    if (!preset_to_use) {
        BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: No preset available, proceeding to fallback search";
        // We'll continue to the fallback logic below
        BOOST_LOG_TRIVIAL(warning) << "=== extract_material_guid_from_preset END (NO PRESET) ===";
        return "";
    }
    
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Selected preset name='" << preset_to_use->name << "'";
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Inherits field='" << preset_to_use->inherits() << "'";
    
        // Helper lambda to extract GUID from a preset's filament_notes
        auto extract_guid_from_preset = [](const Slic3r::Preset* preset) -> std::string {
            if (!preset) return "";
            
            // FIX: filament_notes is ConfigOptionStrings (array), not ConfigOptionString (single value)
            if (const Slic3r::ConfigOptionStrings* notes_opt = preset->config.option<Slic3r::ConfigOptionStrings>("filament_notes")) {
                if (!notes_opt->values.empty()) {
                    std::string notes = notes_opt->values[0];
            
            // Check if MATERIAL_GUID exists in notes
            const std::string guid_tag = "MATERIAL_GUID:";
            size_t guid_pos = notes.find(guid_tag);
            if (guid_pos != std::string::npos) {
                size_t guid_start = guid_pos + guid_tag.length();
                size_t guid_end = notes.find_first_of("\n\r", guid_start);
                if (guid_end == std::string::npos) guid_end = notes.length();
                std::string material_guid = notes.substr(guid_start, guid_end - guid_start);
                boost::algorithm::trim(material_guid);
                if (!material_guid.empty()) {
                    BOOST_LOG_TRIVIAL(debug) << "extract_guid_from_preset: Found GUID '" << material_guid << "' in preset '" << preset->name << "'";
                    return material_guid;
                }
            }
        }
        }
        BOOST_LOG_TRIVIAL(debug) << "extract_guid_from_preset: No GUID found in preset '" << preset->name << "'";
        return "";
    };
    
    // STEP 1: First, check if the selected preset itself has a GUID
    // This handles user-created presets that might have their own GUID
    std::string selected_guid = extract_guid_from_preset(preset_to_use);
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: STEP 1 - Selected preset '" << preset_to_use->name 
                              << "' GUID: " << (selected_guid.empty() ? "[EMPTY]" : selected_guid);
    
    // STEP 2: Walk up the inheritance chain to find a preset with a GUID
    // Start with the selected preset's parent
    const Slic3r::Preset* current = preset_to_use;
    const Slic3r::Preset* base_preset = nullptr;
    std::string base_preset_name = "none";
    std::vector<std::string> inheritance_chain;
    
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: STEP 2 - Walking inheritance chain...";
    
    while (current) {
        // Try to get the parent preset
        const Slic3r::Preset* parent = nullptr;
        if (!current->inherits().empty()) {
            inheritance_chain.push_back(current->name + " (inherits: " + current->inherits() + ")");
            try {
                Slic3r::PresetCollection& filaments = const_cast<Slic3r::PresetCollection&>(preset_bundle->filaments);
                parent = filaments.find_preset(current->inherits(), false, true);
                BOOST_LOG_TRIVIAL(debug) << "extract_material_guid_from_preset: Found parent '" << current->inherits() << "' = " << (parent ? parent->name : "NULL");
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Exception finding parent: " << e.what();
            }
        } else {
            inheritance_chain.push_back(current->name + " (no inherits)");
            BOOST_LOG_TRIVIAL(debug) << "extract_material_guid_from_preset: Preset '" << current->name << "' has no inherits field";
        }
        
        if (!parent) {
            BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: End of inheritance chain, using get_preset_base() as fallback";
            try {
                base_preset = preset_bundle->filaments.get_preset_base(*preset_to_use);
                base_preset_name = base_preset ? base_preset->name : "NULL";
                BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: get_preset_base() returned: '" << base_preset_name << "'";
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Exception in get_preset_base: " << e.what();
                base_preset = nullptr;
            }
            break;
        }
        
        BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Checking parent preset: '" << parent->name << "' (child: '" << current->name << "')";
        
        // Check if this parent has a GUID
        std::string parent_guid = extract_guid_from_preset(parent);
        if (!parent_guid.empty()) {
            base_preset = parent;
            base_preset_name = parent->name;
            BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: SUCCESS - Found GUID in parent '" << parent->name << "': " << parent_guid;
            break;
        }
        
        // Move up the chain
        current = parent;
    }
    
    // Log the full inheritance chain
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Inheritance chain: " << inheritance_chain.size() << " levels";
    for (size_t i = 0; i < inheritance_chain.size(); i++) {
        BOOST_LOG_TRIVIAL(warning) << "  Chain[" << i << "]: " << inheritance_chain[i];
    }
    
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Final base preset: '" << base_preset_name << "'";
    
    // STEP 3: Extract GUID from the base preset we found
    if (base_preset) {
        std::string base_guid = extract_guid_from_preset(base_preset);
        if (!base_guid.empty()) {
            BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: SUCCESS - Base preset '" << base_preset->name << "' has GUID: " << base_guid;
            BOOST_LOG_TRIVIAL(warning) << "=== extract_material_guid_from_preset END (SUCCESS from base) ===";
            return base_guid;
        } else {
            BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Base preset '" << base_preset->name << "' has no GUID";
        }
    } else {
        BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: base_preset is NULL";
    }
    
    // STEP 4: If we still don't have a GUID, return the selected preset's GUID if it exists
    if (!selected_guid.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Using selected preset's own GUID: " << selected_guid;
        BOOST_LOG_TRIVIAL(warning) << "=== extract_material_guid_from_preset END (SUCCESS from selected) ===";
        return selected_guid;
    }
    
    // STEP 5: FALLBACK - Try to find any base preset with "PLA" or "Tough" in the name
    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: STEP 5 - Trying fallback search for PLA-related presets...";
    {
        Slic3r::PresetCollection& filaments = const_cast<Slic3r::PresetCollection&>(preset_bundle->filaments);
        for (const auto& preset : filaments) {
            if (preset.is_system && preset.is_visible) {
                std::string pname = preset.name;
                if (pname.find("PLA") != std::string::npos || pname.find("Tough") != std::string::npos) {
                    std::string pguid = extract_guid_from_preset(&preset);
                    BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Fallback candidate: '" << preset.name << "' GUID: " << (pguid.empty() ? "[NONE]" : pguid);
                    // FIX: Removed `&& base_preset == nullptr` condition that was preventing fallback from working
                    // when a base_preset was already found (even if it had no GUID)
                    if (!pguid.empty()) {
                        base_preset = &preset;
                        base_preset_name = preset.name;
                        BOOST_LOG_TRIVIAL(warning) << "extract_material_guid_from_preset: Fallback - Using preset '" << preset.name << "' with GUID: " << pguid;
                        BOOST_LOG_TRIVIAL(warning) << "=== extract_material_guid_from_preset END (SUCCESS from fallback) ===";
                        return pguid;
                    }
                }
            }
        }
    }
    
    BOOST_LOG_TRIVIAL(error) << "extract_material_guid_from_preset: FAILURE - No GUID found in inheritance chain for preset '" << preset_name_for_debug << "'";
    BOOST_LOG_TRIVIAL(warning) << "=== extract_material_guid_from_preset END (FAILURE) ===";
    return "";
}

namespace Slic3r {

bool SlicingProcessCompletedEvent::critical_error() const
{
	try {
		this->rethrow_exception();
	} catch (const Slic3r::SlicingError &) {
		// Exception derived from SlicingError is non-critical.
		return false;
    } catch (const Slic3r::SlicingErrors &) {
        return false;
    } catch (...) {}
    return true;
}

bool SlicingProcessCompletedEvent::invalidate_plater() const
{
	if (critical_error())
	{
		try {
			this->rethrow_exception();
		}
		catch (const Slic3r::ExportError&) {
			// Exception thrown by copying file does not ivalidate plater
			return false;
		}
		catch (...) {
		}
		return true;
	}
	return false;
}

std::pair<std::string, std::vector<size_t>> SlicingProcessCompletedEvent::format_error_message() const
{
	std::string error;
    size_t      monospace = 0;
	try {
		this->rethrow_exception();
    } catch (const std::bad_alloc &ex) {
        wxString errmsg = GUI::from_u8(boost::format(_utf8(L("A error occurred. Maybe memory of system is not enough or it's a bug "
			                  "of the program"))).str());
        error = std::string(errmsg.ToUTF8()) + "\n" + std::string(ex.what());
    } catch (const HardCrash &ex) {
        error = GUI::format(_u8L("A fatal error occurred: \"%1%\""), ex.what()) + "\n" +
                            _u8L("Please save project and restart the program.");
    } catch (PlaceholderParserError &ex) {
		error = ex.what();
		monospace = 1;
    } catch (SlicingError &ex) {
		error = ex.what();
		monospace = ex.objectId();
    } catch (SlicingErrors &exs) {
        std::vector<size_t> ids;
        for (auto &ex : exs.errors_) {
            error     = ex.what();
            monospace = ex.objectId();
            ids.push_back(monospace);
        }
        return std::make_pair(std::move(error), ids);
    } catch (std::exception &ex) {
        error = ex.what();
    } catch (...) {
        error = "Unknown C++ exception.";
    }
    return std::make_pair(std::move(error), std::vector<size_t>{monospace});
}

BackgroundSlicingProcess::BackgroundSlicingProcess()
{
	//BBS: move this logic to part plate
#if 0
    boost::filesystem::path temp_path(wxStandardPaths::Get().GetTempDir().utf8_str().data());
    temp_path /= (boost::format(".%1%.gcode") % get_current_pid()).str();
	m_temp_output_path = temp_path.string();
#endif
}

BackgroundSlicingProcess::~BackgroundSlicingProcess()
{
	this->stop();
	this->join_background_thread();
	//BBS: move this logic to part plate
	//boost::nowide::remove(m_temp_output_path.c_str());
}

//BBS: switch the print in background slicing process
bool BackgroundSlicingProcess::switch_print_preprocess()
{
	bool result = true;

	/*switch (m_printer_tech) {
	case ptFFF: m_print = m_fff_print; break;
	case ptSLA: m_print = m_sla_print; break;
	default: assert(false); break;
	}*/
	return result;
}

//BBS: judge whether can switch the print
bool BackgroundSlicingProcess::can_switch_print()
{
	bool result = true;

	if (m_state == STATE_RUNNING)
	{
		//currently it is on slicing, judge whether the slice result is valid or not
		//if (m_current_plate->is_slice_result_valid())
		{
			result = false;
			BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": slicing plate's plate_id %1%, on slicing, can not switch print") % m_current_plate->get_index();
		}
	}

	return result;
}

//BBS: select the printer technology
bool BackgroundSlicingProcess::select_technology(PrinterTechnology tech)
{
	bool changed = false;
	if (m_printer_tech != tech) {
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": change the printer technology from %1% to %2%") % m_printer_tech % tech;
		m_printer_tech = tech;
		if (m_print != nullptr)
			this->reset();
		changed = true;
	}

	switch (tech) {
	case ptFFF: m_print = m_fff_print; break;
	case ptSLA: m_print = m_sla_print; break;
	default: assert(false); break;
	}
	assert(m_print != nullptr);
	return changed;
}

PrinterTechnology BackgroundSlicingProcess::current_printer_technology() const
{
	//BBS: as the m_printer is changed frequently when switch plates, use m_printer_tech directly
	return m_printer_tech;
	//return m_print->technology();
}

std::string BackgroundSlicingProcess::output_filepath_for_project(const boost::filesystem::path &project_path)
{
	assert(m_print != nullptr);
    if (project_path.empty())
        return m_print->output_filepath("");
    return m_print->output_filepath(project_path.parent_path().string(), project_path.stem().string());
}

// This function may one day be merged into the Print, but historically the print was separated
// from the G-code generator.
void BackgroundSlicingProcess::process_fff()
{
    assert(m_print == m_fff_print);
    PresetBundle &preset_bundle = *wxGetApp().preset_bundle;
    m_fff_print->is_BBL_printer() = preset_bundle.is_bbl_vendor();
	//BBS: add the logic to process from an existed gcode file
	if (m_print->finished()) {
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: skip slicing, to process previous gcode file")%__LINE__;
		m_fff_print->set_status(80, _utf8(L("Processing G-code from Previous file...")));
		wxCommandEvent evt(m_event_slicing_completed_id);
		// Post the Slicing Finished message for the G-code viewer to update.
		// Passing the timestamp 
		evt.SetInt((int)(m_fff_print->step_state_with_timestamp(PrintStep::psSlicingFinished).timestamp));
		wxQueueEvent(GUI::wxGetApp().mainframe->m_plater, evt.Clone());

		m_temp_output_path = this->get_current_plate()->get_tmp_gcode_path();
		if (! m_export_path.empty()) {
			BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: export gcode from %2% directly to %3%")%__LINE__%m_temp_output_path %m_export_path;
		}
		else {
            if (m_upload_job.empty()) {
                m_fff_print->export_gcode_from_previous_file(m_temp_output_path, m_gcode_result, [this](const ThumbnailsParams &params) {
                    return this->render_thumbnails(params);
                });
            }
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: export_gcode_from_previous_file from %2% finished")%__LINE__ % m_temp_output_path;
		}
	}
	else {
		//BBS: reset the gcode before reload_print in slicing_completed event processing
		//FIX the gcode rename failed issue
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: will start slicing, reset gcode_result %2% firstly")%__LINE__%m_gcode_result;
		m_gcode_result->reset();

		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: gcode_result reseted, will start print::process")%__LINE__;
		m_print->process();
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" %1%: after print::process, send slicing complete event to gui...")%__LINE__;
        if (m_current_plate->get_real_filament_map_mode(preset_bundle.project_config) < FilamentMapMode::fmmManual) {
            std::vector<int> f_maps = m_fff_print->get_filament_maps();
            m_current_plate->set_filament_maps(f_maps);
		}
		wxCommandEvent evt(m_event_slicing_completed_id);
		// Post the Slicing Finished message for the G-code viewer to update.
		// Passing the timestamp
		evt.SetInt((int)(m_fff_print->step_state_with_timestamp(PrintStep::psSlicingFinished).timestamp));
		wxQueueEvent(GUI::wxGetApp().mainframe->m_plater, evt.Clone());

		//BBS: add plate index into render params
		m_temp_output_path = this->get_current_plate()->get_tmp_gcode_path();
		m_fff_print->export_gcode(m_temp_output_path, m_gcode_result, [this](const ThumbnailsParams& params) { return this->render_thumbnails(params); });
		if(m_fff_print->is_BBL_printer())
			run_post_process_scripts(m_temp_output_path, false, "File", m_temp_output_path, m_fff_print->full_print_config());

		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": export gcode finished");
	}
	if (this->set_step_started(bspsGCodeFinalize)) {
	    if (! m_export_path.empty()) {
			wxQueueEvent(GUI::wxGetApp().mainframe->m_plater, new wxCommandEvent(m_event_export_began_id));
			if(!m_fff_print->is_BBL_printer())
				finalize_gcode();
			else
				export_gcode();
	    } else if (! m_upload_job.empty()) {
			wxQueueEvent(GUI::wxGetApp().mainframe->m_plater, new wxCommandEvent(m_event_export_began_id));
			prepare_upload();
	    } else {
			m_print->set_status(100, _utf8(L("Slicing complete")));
	    }
		this->set_step_done(bspsGCodeFinalize);
	}
}

static void write_thumbnail(Zipper& zipper, const ThumbnailData& data)
{
    size_t png_size = 0;
    void* png_data = tdefl_write_image_to_png_file_in_memory_ex((const void*)data.pixels.data(), data.width, data.height, 4, &png_size, MZ_DEFAULT_LEVEL, 1);
    if (png_data != nullptr)
    {
        zipper.add_entry("thumbnail/thumbnail" + std::to_string(data.width) + "x" + std::to_string(data.height) + ".png", (const std::uint8_t*)png_data, png_size);
        mz_free(png_data);
    }
}

void BackgroundSlicingProcess::process_sla()
{
    assert(m_print == m_sla_print);
    m_print->process();
    if (this->set_step_started(bspsGCodeFinalize)) {
        if (! m_export_path.empty()) {
			wxQueueEvent(GUI::wxGetApp().mainframe->m_plater, new wxCommandEvent(m_event_export_began_id));

            const std::string export_path = m_sla_print->print_statistics().finalize_output_path(m_export_path);

			//BBS: add plate id for thumbnail generation
            ThumbnailsList thumbnails = this->render_thumbnails(
				ThumbnailsParams{ current_print()->full_print_config().option<ConfigOptionPoints>("thumbnails")->values, true, true, true, true, 0 });

            Zipper zipper(export_path);
            m_sla_archive.export_print(zipper, *m_sla_print);																											         // true, false, true, true); // renders also supports and pad
			for (const ThumbnailData& data : thumbnails)
                if (data.is_valid())
                    write_thumbnail(zipper, data);
            zipper.finalize();

            //m_print->set_status(100, (boost::format(_utf8(L("Masked SLA file exported to %1%"))) % export_path).str());
			m_print->set_status(100, (boost::format(_utf8("Masked SLA file exported to %1%")) % export_path).str());
        } else {
			//m_print->set_status(100, _utf8(L("Slicing complete")));
			m_print->set_status(100, _utf8("Slicing complete"));
        }
        this->set_step_done(bspsGCodeFinalize);
    }
}

void BackgroundSlicingProcess::thread_proc()
{
	//BBS: thread name
	set_current_thread_name("bbl_BgSlcPcs");
    name_tbb_thread_pool_threads_set_locale();

	assert(m_print != nullptr);
	assert(m_print == m_fff_print || m_print == m_sla_print);
	std::unique_lock<std::mutex> lck(m_mutex);
	// Let the caller know we are ready to run the background processing task.
	m_state = STATE_IDLE;
	lck.unlock();
	m_condition.notify_one();
	for (;;) {
		//BBS: sometimes the state has already been set in the start function
		//assert(m_state == STATE_IDLE || m_state == STATE_CANCELED || m_state == STATE_FINISHED || m_state == STATE_STARTED);
		// Wait until a new task is ready to be executed, or this thread should be finished.
		lck.lock();
		m_condition.wait(lck, [this](){ return m_state == STATE_STARTED || m_state == STATE_EXIT; });
		if (m_state == STATE_EXIT)
			// Exiting this thread.
			break;
		// Process the background slicing task.
		m_state = STATE_RUNNING;
		//BBS: internal cancel
		m_internal_cancelled = false;
		lck.unlock();
		std::exception_ptr exception;
#ifdef _WIN32
		this->call_process_seh_throw(exception);
#else
		this->call_process(exception);
#endif
		m_print->finalize();
		lck.lock();
		m_state = m_print->canceled() ? STATE_CANCELED : STATE_FINISHED;
		BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(": process finished, state %1%, print cancel_status %2%")%m_state %m_print->cancel_status();
		if (m_print->cancel_status() != Print::CANCELED_INTERNAL) {
			// Only post the canceled event, if canceled by user.
			// Don't post the canceled event, if canceled from Print::apply().
			SlicingProcessCompletedEvent evt(m_event_finished_id, 0,
				(m_state == STATE_CANCELED) ? SlicingProcessCompletedEvent::Cancelled :
				exception ? SlicingProcessCompletedEvent::Error : SlicingProcessCompletedEvent::Finished, exception);
			BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": send SlicingProcessCompletedEvent to main, status %1%")%evt.status();
			wxQueueEvent(GUI::wxGetApp().mainframe->m_plater, evt.Clone());
		}
		else {
			//BBS: internal cancel
			m_internal_cancelled = true;
		}
		m_print->restart();
		lck.unlock();
		// Let the UI thread wake up if it is waiting for the background task to finish.
		m_condition.notify_one();
		// Let the UI thread see the result.
	}
	m_state = STATE_EXITED;
	lck.unlock();
	// End of the background processing thread. The UI thread should join m_thread now.
}

#ifdef _WIN32
// Only these SEH exceptions will be catched and turned into Slic3r::HardCrash C++ exceptions.
static bool is_win32_seh_harware_exception(unsigned long ex) throw() {
	return
		ex == STATUS_ACCESS_VIOLATION ||
		ex == STATUS_DATATYPE_MISALIGNMENT ||
		ex == STATUS_FLOAT_DIVIDE_BY_ZERO ||
		ex == STATUS_FLOAT_OVERFLOW ||
		ex == STATUS_FLOAT_UNDERFLOW ||
#ifdef STATUS_FLOATING_RESEVERED_OPERAND
		ex == STATUS_FLOATING_RESEVERED_OPERAND ||
#endif // STATUS_FLOATING_RESEVERED_OPERAND
		ex == STATUS_ILLEGAL_INSTRUCTION ||
		ex == STATUS_PRIVILEGED_INSTRUCTION ||
		ex == STATUS_INTEGER_DIVIDE_BY_ZERO ||
		ex == STATUS_INTEGER_OVERFLOW ||
		ex == STATUS_STACK_OVERFLOW;
}

// Rethrow some SEH exceptions as Slic3r::HardCrash C++ exceptions.
static void rethrow_seh_exception(unsigned long win32_seh_catched)
{
	if (win32_seh_catched) {
		// Rethrow SEH exception as Slicer::HardCrash.
		if (win32_seh_catched == STATUS_ACCESS_VIOLATION || win32_seh_catched == STATUS_DATATYPE_MISALIGNMENT)
			throw Slic3r::HardCrash(_u8L("Access violation"));
		if (win32_seh_catched == STATUS_ILLEGAL_INSTRUCTION || win32_seh_catched == STATUS_PRIVILEGED_INSTRUCTION)
			throw Slic3r::HardCrash(_u8L("Illegal instruction"));
		if (win32_seh_catched == STATUS_FLOAT_DIVIDE_BY_ZERO || win32_seh_catched == STATUS_INTEGER_DIVIDE_BY_ZERO)
			throw Slic3r::HardCrash(_u8L("Divide by zero"));
		if (win32_seh_catched == STATUS_FLOAT_OVERFLOW || win32_seh_catched == STATUS_INTEGER_OVERFLOW)
			throw Slic3r::HardCrash(_u8L("Overflow"));
		if (win32_seh_catched == STATUS_FLOAT_UNDERFLOW)
			throw Slic3r::HardCrash(_u8L("Underflow"));
#ifdef STATUS_FLOATING_RESEVERED_OPERAND
		if (win32_seh_catched == STATUS_FLOATING_RESEVERED_OPERAND)
			throw Slic3r::HardCrash(_u8L("Floating reserved operand"));
#endif // STATUS_FLOATING_RESEVERED_OPERAND
		if (win32_seh_catched == STATUS_STACK_OVERFLOW)
			throw Slic3r::HardCrash(_u8L("Stack overflow"));
	}
}

// Wrapper for Win32 structured exceptions. Win32 structured exception blocks and C++ exception blocks cannot be mixed in the same function.
unsigned long BackgroundSlicingProcess::call_process_seh(std::exception_ptr &ex) throw()
{
	unsigned long win32_seh_catched = 0;
	__try {
		this->call_process(ex);
	} __except (is_win32_seh_harware_exception(GetExceptionCode())) {
		win32_seh_catched = GetExceptionCode();
	}
	return win32_seh_catched;
}
void BackgroundSlicingProcess::call_process_seh_throw(std::exception_ptr &ex) throw()
{
	unsigned long win32_seh_catched = this->call_process_seh(ex);
	if (win32_seh_catched) {
		// Rethrow SEH exception as Slicer::HardCrash.
		try {
			rethrow_seh_exception(win32_seh_catched);
		} catch (...) {
			ex = std::current_exception();
		}
	}
}
#endif // _WIN32

void BackgroundSlicingProcess::call_process(std::exception_ptr &ex) throw()
{
	try {
		assert(m_print != nullptr);
		switch (m_print->technology()) {
		case ptFFF: this->process_fff(); break;
		case ptSLA: this->process_sla(); break;
		default: m_print->process(); break;
		}
	} catch (CanceledException& /* ex */) {
		// Canceled, this is all right.
		assert(m_print->canceled());
		ex = std::current_exception();
		BOOST_LOG_TRIVIAL(error) <<__FUNCTION__ << ":got cancelled exception" << std::endl;
	} catch (...) {
		ex = std::current_exception();
		BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ":got other exception" << std::endl;
	}
}

#ifdef _WIN32
unsigned long BackgroundSlicingProcess::thread_proc_safe_seh() throw()
{
	unsigned long win32_seh_catched = 0;
	__try {
		this->thread_proc_safe();
	} __except (is_win32_seh_harware_exception(GetExceptionCode())) {
		win32_seh_catched = GetExceptionCode();
	}
	return win32_seh_catched;
}
void BackgroundSlicingProcess::thread_proc_safe_seh_throw() throw()
{
	unsigned long win32_seh_catched = this->thread_proc_safe_seh();
	if (win32_seh_catched) {
		// Rethrow SEH exception as Slicer::HardCrash.
		try {
			rethrow_seh_exception(win32_seh_catched);
		} catch (...) {
			wxTheApp->OnUnhandledException();
		}
	}
}
#endif // _WIN32

void BackgroundSlicingProcess::thread_proc_safe() throw()
{
	try {
		this->thread_proc();
	} catch (...) {
		wxTheApp->OnUnhandledException();
   	}
}

void BackgroundSlicingProcess::join_background_thread()
{
	std::unique_lock<std::mutex> lck(m_mutex);
	if (m_state == STATE_INITIAL) {
		// Worker thread has not been started yet.
		assert(! m_thread.joinable());
	} else {
		assert(m_state == STATE_IDLE);
		assert(m_thread.joinable());
		// Notify the worker thread to exit.
		m_state = STATE_EXIT;
		lck.unlock();
		m_condition.notify_one();
		// Wait until the worker thread exits.
		m_thread.join();
	}
}

bool BackgroundSlicingProcess::start()
{
	if (m_print->empty()) {
		if (!m_current_plate  || !m_current_plate->is_slice_result_valid())
			// The print is empty (no object in Model, or all objects are out of the print bed).
		    return false;
	}

	std::unique_lock<std::mutex> lck(m_mutex);
	if (m_state == STATE_INITIAL) {
		// The worker thread is not running yet. Start it.
		assert(! m_thread.joinable());
		m_thread = create_thread([this]{
#ifdef _WIN32
			this->thread_proc_safe_seh_throw();
#else // _WIN32
			this->thread_proc_safe();
#endif // _WIN32
		});
		// Wait until the worker thread is ready to execute the background processing task.
		m_condition.wait(lck, [this](){ return m_state == STATE_IDLE; });
	}
	assert(m_state == STATE_IDLE || this->running());
	if (this->running())
		// The background processing thread is already running.
		return false;
	if (! this->idle())
		throw Slic3r::RuntimeError("Cannot start a background task, the worker thread is not idle.");
	m_state = STATE_STARTED;
	m_print->set_cancel_callback([this](){ this->stop_internal(); });
	lck.unlock();
	m_condition.notify_one();
	return true;
}

// To be called on the UI thread.
bool BackgroundSlicingProcess::stop()
{
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", enter"<<std::endl;
	// m_print->state_mutex() shall NOT be held. Unfortunately there is no interface to test for it.
	std::unique_lock<std::mutex> lck(m_mutex);
	if (m_state == STATE_INITIAL) {
//		m_export_path.clear();
		return false;
	}
//	assert(this->running());
	if (m_state == STATE_STARTED || m_state == STATE_RUNNING) {
		// Cancel any task planned by the background thread on UI thread.
		cancel_ui_task(m_ui_task);
		m_print->cancel();
		// Wait until the background processing stops by being canceled.
		m_condition.wait(lck, [this](){ return m_state == STATE_CANCELED; });
		// In the "Canceled" state. Reset the state to "Idle".
		m_state = STATE_IDLE;
		m_print->set_cancel_callback([](){});
	} else if (m_state == STATE_FINISHED || m_state == STATE_CANCELED) {
		// In the "Finished" or "Canceled" state. Reset the state to "Idle".
		m_state = STATE_IDLE;
		m_print->set_cancel_callback([](){});
	}
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", exit"<<std::endl;
//	m_export_path.clear();
	return true;
}

bool BackgroundSlicingProcess::reset()
{
	bool stopped = this->stop();
	this->reset_export();
	//BBS: don't clear print for print is not owned by background slicing process anymore
	//do it in the part_plate
	//m_print->clear();
	this->invalidate_all_steps();
	return stopped;
}

// To be called by Print::apply() on the UI thread through the Print::m_cancel_callback to stop the background
// processing before changing any data of running or finalized milestones.
// This function shall not trigger any UI update through the wxWidgets event.
void BackgroundSlicingProcess::stop_internal()
{
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", enter"<<std::endl;
	// m_print->state_mutex() shall be held. Unfortunately there is no interface to test for it.
	if (m_state == STATE_IDLE)
		// The worker thread is waiting on m_mutex/m_condition for wake up. The following lock of the mutex would block.
		return;
	std::unique_lock<std::mutex> lck(m_mutex);
	assert(m_state == STATE_STARTED || m_state == STATE_RUNNING || m_state == STATE_FINISHED || m_state == STATE_CANCELED);
	if (m_state == STATE_STARTED || m_state == STATE_RUNNING) {
		// Cancel any task planned by the background thread on UI thread.
		cancel_ui_task(m_ui_task);
		// At this point of time the worker thread may be blocking on m_print->state_mutex().
		// Set the print state to canceled before unlocking the state_mutex(), so when the worker thread wakes up,
		// it throws the CanceledException().
		m_print->cancel_internal();
		// Allow the worker thread to wake up if blocking on a milestone.
		m_print->state_mutex().unlock();
		// Wait until the background processing stops by being canceled.
		m_condition.wait(lck, [this](){ return m_state == STATE_CANCELED; });
		// Lock it back to be in a consistent state.
		m_print->state_mutex().lock();
	}
	// In the "Canceled" state. Reset the state to "Idle".
	m_state = STATE_IDLE;
	m_print->set_cancel_callback([](){});
	BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ", exit"<<std::endl;
}

// Execute task from background thread on the UI thread. Returns true if processed, false if cancelled.
bool BackgroundSlicingProcess::execute_ui_task(std::function<void()> task)
{
	bool running = false;
	if (m_mutex.try_lock()) {
		// Cancellation is either not in process, or already canceled and waiting for us to finish.
		// There must be no UI task planned.
		assert(! m_ui_task);
		if (! m_print->canceled()) {
			running = true;
			m_ui_task = std::make_shared<UITask>();
		}
		m_mutex.unlock();
	} else {
		// Cancellation is in process.
	}

	bool result = false;
	if (running) {
		std::shared_ptr<UITask> ctx = m_ui_task;
		GUI::wxGetApp().mainframe->m_plater->CallAfter([task, ctx]() {
			// Running on the UI thread, thus ctx->state does not need to be guarded with mutex against ::cancel_ui_task().
			assert(ctx->state == UITask::Planned || ctx->state == UITask::Canceled);
			if (ctx->state == UITask::Planned) {
				task();
				std::unique_lock<std::mutex> lck(ctx->mutex);
	    		ctx->state = UITask::Finished;
	    	}
	    	// Wake up the worker thread from the UI thread.
    		ctx->condition.notify_all();
	    });

	    {
			std::unique_lock<std::mutex> lock(ctx->mutex);
	    	ctx->condition.wait(lock, [&ctx]{ return ctx->state == UITask::Finished || ctx->state == UITask::Canceled; });
	    }
	    result = ctx->state == UITask::Finished;
		m_ui_task.reset();
	}

	return result;
}

// To be called on the UI thread from ::stop() and ::stop_internal().
void BackgroundSlicingProcess::cancel_ui_task(std::shared_ptr<UITask> task)
{
	if (task) {
		std::unique_lock<std::mutex> lck(task->mutex);
		task->state = UITask::Canceled;
		lck.unlock();
		task->condition.notify_all();
	}
}

// Shared helper to build UFP container - used by both export_to_final_path() and prepare_upload()
// This ensures consistent UFP generation and avoids code duplication
bool BackgroundSlicingProcess::build_ufp_container(const std::string& gcode_path,
                                                    const std::string& output_path,
                                                    const std::string& printer_notes,
                                                    std::string& error_message)
{
    BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: ENTER - gcode=" << gcode_path << ", output=" << output_path;
    
    // Check for nullptrs early to prevent access violations
    if (!m_fff_print) {
        BOOST_LOG_TRIVIAL(error) << "build_ufp_container: m_fff_print is NULL!";
        error_message = "Internal error: m_fff_print is null";
        return false;
    }
    
    // Determine format type from printer notes
    std::string format_type = Slic3r::FormatConfig::get_format_type_for_printer(printer_notes);
    if (format_type.empty()) {
        error_message = "No valid format type found in printer notes";
        BOOST_LOG_TRIVIAL(error) << "build_ufp_container: " << error_message;
        return false;
    }
    
    BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: format_type=" << format_type;
    
    // Extract extruder variants from print config for multi-extruder support
    std::vector<std::string> extruder_variants;
    if (const ConfigOptionStrings* variant_opt = m_fff_print->full_print_config().option<ConfigOptionStrings>("printer_extruder_variant")) {
        extruder_variants = variant_opt->values;
        BOOST_LOG_TRIVIAL(info) << "build_ufp_container: Found " << extruder_variants.size() << " extruder variants";
    }
    
    // Extract extruder data (GUIDs, temps, volumes) for multi-extruder UFP export
    std::vector<Slic3r::ExtruderData> extruder_data;
    const DynamicPrintConfig& full_config = m_fff_print->full_print_config();
    
    // Get filament presets to compare for single/multi-material detection
    const ConfigOptionStrings* filament_preset_opts = full_config.option<ConfigOptionStrings>("filament_presets");
    std::vector<std::string> filament_preset_values;
    if (filament_preset_opts) {
        filament_preset_values = filament_preset_opts->values;
    }
    
    // Get filament presets
    if (const ConfigOptionStrings* filament_opts = full_config.option<ConfigOptionStrings>("filament_type")) {
        const std::vector<std::string>& filament_values = filament_opts->values;
        
        // Get filament temps if available
        std::vector<int> extruder_temps;
        if (const ConfigOptionInts* temp_opts = full_config.option<ConfigOptionInts>("nozzle_temperature")) {
            extruder_temps = temp_opts->values;
        }
        
        // Build extruder data for each active extruder (up to 2)
        for (size_t i = 0; i < filament_values.size() && i < 2; ++i) {
            Slic3r::ExtruderData edata;
            
            // Look up the filament preset for GUID and brand extraction
            const Slic3r::Preset* filament_preset = nullptr;
            
            // CRITICAL: Use m_preset_bundle captured from UI thread to avoid thread safety issues
            // wxGetApp().preset_bundle may change or become invalid when accessed from background thread
            BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Searching for filament preset: " 
                                     << (i < filament_preset_values.size() ? filament_preset_values[i] : "(empty)");
            
            const Slic3r::PresetBundle* preset_bundle = m_preset_bundle;
            if (!preset_bundle) {
                BOOST_LOG_TRIVIAL(error) << "build_ufp_container: m_preset_bundle is NULL! (thread safety issue)";
                // Continue with empty GUID rather than crashing
            } else {
                try {
                    if (i < filament_preset_values.size() && !filament_preset_values[i].empty()) {
                        std::string preset_name = filament_preset_values[i];
                        filament_preset = preset_bundle->filaments.find_preset(preset_name, false);
                        BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Preset lookup result: "
                                                 << (filament_preset ? "found: " + filament_preset->name : "NOT FOUND");
                    }
                    
                    // Fallback: If preset name is empty but filament_type exists, try multiple strategies to find preset
                    if (!filament_preset && i < filament_values.size() && !filament_values[i].empty()) {
                        std::string filament_type = filament_values[i];
                        BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Fallback lookup for filament_type: " << filament_type;
                        
                        // Strategy 1: Try filament_id_by_type first (standard lookup)
                        std::string filament_id = preset_bundle->filaments.filament_id_by_type(filament_type);
                        if (!filament_id.empty()) {
                            filament_preset = preset_bundle->filaments.find_preset(filament_id, false);
                            BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Strategy 1 (filament_id): " 
                                                     << filament_type << " -> " << filament_id
                                                     << " -> " << (filament_preset ? "found: " + filament_preset->name : "NOT FOUND");
                        }
                        
                        // Strategy 2: If that fails, search all presets for matching filament_type field
                        // Use two-pass approach to prioritize presets with MATERIAL_GUID (for manufacturer-specific materials)
                        if (!filament_preset) {
                            BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Strategy 2 - searching presets by filament_type field: " << filament_type;
                            
                            // Helper lambda to extract GUID from a preset's filament_notes
                            // This is a local copy of the same logic used in extract_material_guid_from_preset
                            auto extract_guid_from_preset_local = [](const Slic3r::Preset* preset) -> std::string {
                                if (!preset) return "";
                                
                                // filament_notes is ConfigOptionStrings (array), not ConfigOptionString (single value)
                                if (const Slic3r::ConfigOptionStrings* notes_opt = preset->config.option<Slic3r::ConfigOptionStrings>("filament_notes")) {
                                    if (!notes_opt->values.empty()) {
                                        std::string notes = notes_opt->values[0];
                                
                                        // Check if MATERIAL_GUID exists in notes
                                        const std::string guid_tag = "MATERIAL_GUID:";
                                        size_t guid_pos = notes.find(guid_tag);
                                        if (guid_pos != std::string::npos) {
                                            size_t guid_start = guid_pos + guid_tag.length();
                                            size_t guid_end = notes.find_first_of("\n\r", guid_start);
                                            if (guid_end == std::string::npos) guid_end = notes.length();
                                            std::string material_guid = notes.substr(guid_start, guid_end - guid_start);
                                            boost::algorithm::trim(material_guid);
                                            if (!material_guid.empty()) {
                                                return material_guid;
                                            }
                                        }
                                    }
                                }
                                return "";
                            };
                            
                            // First pass: Find a preset with matching filament_type AND has a MATERIAL_GUID
                            for (const auto& preset : preset_bundle->filaments) {
                                if (preset.is_system && preset.is_visible) {
                                    if (const Slic3r::ConfigOptionStrings* type_opt = preset.config.option<Slic3r::ConfigOptionStrings>("filament_type")) {
                                        if (!type_opt->values.empty() && type_opt->values[0] == filament_type) {
                                            // Check if this preset has a MATERIAL_GUID
                                            std::string guid = extract_guid_from_preset_local(&preset);
                                            if (!guid.empty()) {
                                                filament_preset = &preset;
                                                BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Found preset with GUID by filament_type field: " << preset.name;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Second pass: If no preset with GUID found, use the first matching preset
                            if (!filament_preset) {
                                BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Strategy 2 - no preset with GUID found, using first match";
                                for (const auto& preset : preset_bundle->filaments) {
                                    if (preset.is_system && preset.is_visible) {
                                        if (const Slic3r::ConfigOptionStrings* type_opt = preset.config.option<Slic3r::ConfigOptionStrings>("filament_type")) {
                                            if (!type_opt->values.empty() && type_opt->values[0] == filament_type) {
                                                filament_preset = &preset;
                                                BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Found preset by filament_type field: " << preset.name;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Strategy 3: Try partial name matching (e.g., "PLA Tough" might match "UltiMaker Tough PLA @base")
                        if (!filament_preset) {
                            BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Strategy 3 - partial name match for: " << filament_type;
                            std::string search_lower = filament_type;
                            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
                            for (const auto& preset : preset_bundle->filaments) {
                                if (preset.is_system && preset.is_visible) {
                                    std::string name_lower = preset.name;
                                    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                                    // Check if filament_type appears in preset name (case-insensitive)
                                    if (name_lower.find(search_lower) != std::string::npos) {
                                        filament_preset = &preset;
                                        BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Found preset by partial name match: " << preset.name;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        if (!filament_preset) {
                            BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: No preset found for filament_type: " << filament_type;
                        }
                    }

                    // Extract material GUID from filament preset (walks inheritance chain to find base preset)
                    std::string material_guid = extract_material_guid_from_preset(filament_preset, preset_bundle, 
                        i < filament_preset_values.size() ? filament_preset_values[i] : 
                        (i < filament_values.size() ? filament_values[i] : "unknown"));
                    
                    // For container formats (.ufp/.makerbot), NEVER use generated GUIDs - they are always invalid
                    if (material_guid.empty()) {
                        BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: No MATERIAL_GUID found for preset, using empty GUID";
                        material_guid = "";
                    }
                    
                    edata.material_guid = material_guid;
                } catch (const std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "build_ufp_container: Exception accessing preset_bundle: " << e.what();
                    edata.material_guid = "";
                }
                
                // Extract brand from filament preset if available
                if (filament_preset) {
                    const Slic3r::Preset* base_preset = preset_bundle->filaments.get_preset_base(*filament_preset);
                    if (!base_preset) base_preset = filament_preset;
                    
                    if (const Slic3r::ConfigOptionString* vendor_opt = base_preset->config.option<Slic3r::ConfigOptionString>("filament_vendor")) {
                        edata.brand = vendor_opt->value;
                        BOOST_LOG_TRIVIAL(info) << "build_ufp_container: Extruder " << i << " brand: " << edata.brand;
                    }
                }
            }
            
            // Fallback brand extraction from print config if preset lookup failed
            if (edata.brand.empty()) {
                const ConfigOptionStrings* vendor_opts = full_config.option<ConfigOptionStrings>("filament_vendor");
                if (vendor_opts && i < vendor_opts->values.size() && !vendor_opts->values[i].empty()) {
                    edata.brand = vendor_opts->values[i];
                    BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Extruder " << i << " brand from config: " << edata.brand;
                }
            }
            
            edata.material_name = (i < filament_values.size()) ? filament_values[i] : "Unknown";
            edata.extruder_temp = (i < extruder_temps.size()) ? extruder_temps[i] : 0;
            edata.filament_mm = 0.0;
            edata.filament_g = 0.0;
            
            BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Extruder " << i << " preset lookup: " 
                                      << (filament_preset ? "found" : "NOT FOUND")
                                      << ", GUID: " << edata.material_guid
                                      << ", brand: " << edata.brand;
            
            extruder_data.push_back(edata);
            BOOST_LOG_TRIVIAL(info) << "build_ufp_container: Extruder " << i 
                                    << " - GUID: " << edata.material_guid
                                    << ", temp: " << edata.extruder_temp
                                    << ", name: " << edata.material_name;
        }
    }
    
    // Get filament length from print statistics
    if (!extruder_data.empty()) {
        double total_filament_mm = m_fff_print->print_statistics().total_used_filament;
        double total_filament_g = m_fff_print->print_statistics().total_weight;
        
        // Check if single or multi-material print
        bool is_single_material = true;
        if (filament_preset_values.size() >= 2) {
            const std::string& p0 = filament_preset_values[0];
            const std::string& p1 = filament_preset_values[1];
            if (!p0.empty() && !p1.empty() && p0 != p1) {
                is_single_material = false;
                BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Multi-material print detected";
            } else {
                BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Single material print detected";
            }
        }
        
        // Distribute filament across extruders
        if (extruder_data.size() == 1 || is_single_material) {
            extruder_data[0].filament_mm = total_filament_mm;
            extruder_data[0].filament_g = total_filament_g;
            if (extruder_data.size() >= 2) {
                extruder_data[1].filament_mm = 0.0;
                extruder_data[1].filament_g = 0.0;
            }
        } else if (extruder_data.size() == 2 && !is_single_material) {
            extruder_data[0].filament_mm = total_filament_mm / 2.0;
            extruder_data[0].filament_g = total_filament_g / 2.0;
            extruder_data[1].filament_mm = total_filament_mm / 2.0;
            extruder_data[1].filament_g = total_filament_g / 2.0;
        }
        
        BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Final filament distribution - extruder 0: " 
                                    << extruder_data[0].filament_mm << "mm, extruder 1: " 
                                    << (extruder_data.size() > 1 ? extruder_data[1].filament_mm : 0.0) << "mm";
    }
    
    // Generate PNG thumbnail for UFP using the plate's cached thumbnail_data.
    // The plate thumbnail is rendered during slicing (at slicing-completed time) and cached;
    // re-rendering via render_thumbnails() at export time is unreliable (OpenGL context state,
    // timing issues) and has been observed to produce a blank white image.
    // Using the cached thumbnail_data is the correct approach.
    std::vector<uint8_t> thumbnail_data;
    if (m_current_plate && m_current_plate->thumbnail_data.is_valid()) {
        auto compressed = GCodeThumbnails::compress_thumbnail(m_current_plate->thumbnail_data, GCodeThumbnailsFormat::PNG);
        if (compressed && compressed->data && compressed->size) {
            thumbnail_data.assign((uint8_t*)compressed->data,
                                  (uint8_t*)compressed->data + compressed->size);
            BOOST_LOG_TRIVIAL(info) << "build_ufp_container: Using cached plate thumbnail, PNG size=" << thumbnail_data.size();
        } else {
            BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Thumbnail PNG compression failed";
        }
    } else {
        BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: No valid cached thumbnail on current plate"
                                   << " (plate=" << (m_current_plate ? "valid" : "null")
                                   << ", is_valid=" << (m_current_plate ? m_current_plate->thumbnail_data.is_valid() : false) << ")"
                                   << " - UFP will have no thumbnail";
    }
    
    // Export to container format
    BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: Converting G-code to container format: " << format_type;
    std::string container_error;
    if (!Slic3r::FormatConfig::export_to_container(format_type, gcode_path, output_path, printer_notes, 
                                                   extruder_variants, extruder_data, thumbnail_data, container_error)) {
        BOOST_LOG_TRIVIAL(error) << "build_ufp_container: ERROR - Container conversion FAILED: " << container_error;
        error_message = "Failed to export in container format.\n" + container_error;
        return false;
    }
    
    BOOST_LOG_TRIVIAL(warning) << "build_ufp_container: SUCCESS - Container created at: " << output_path;
    return true;
}

bool BackgroundSlicingProcess::empty() const
{
	assert(m_print != nullptr);
	return m_print->empty();
}

StringObjectException BackgroundSlicingProcess::validate(StringObjectException *warning, Polygons* collison_polygons, std::vector<std::pair<Polygon, float>>* height_polygons)
{
	assert(m_print != nullptr);
    assert(m_print == m_fff_print);

    m_fff_print->is_BBL_printer() = wxGetApp().preset_bundle->is_bbl_vendor();
    return m_print->validate(warning, collison_polygons, height_polygons);
}

// Apply config over the print. Returns false, if the new config values caused any of the already
// processed steps to be invalidated, therefore the task will need to be restarted.
Print::ApplyStatus BackgroundSlicingProcess::apply(const Model &model, const DynamicPrintConfig &config)
{
	assert(m_print != nullptr);
	assert(config.opt_enum<PrinterTechnology>("printer_technology") == m_print->technology());
	// TODO: add partplate config
	DynamicPrintConfig new_config = config;
	new_config.apply(*m_current_plate->config());
	Print::ApplyStatus invalidated = m_print->apply(model, new_config);

	// Orca: prevent resetting under gcode viewer mode
    if (invalidated != PrintBase::APPLY_STATUS_UNCHANGED) {
        const auto plater = GUI::wxGetApp().mainframe->m_plater;
        if (plater && plater->only_gcode_mode()) {
            invalidated = PrintBase::APPLY_STATUS_UNCHANGED;
        }
    }

	if ((invalidated & PrintBase::APPLY_STATUS_INVALIDATED) != 0 && m_print->technology() == ptFFF &&
		!m_fff_print->is_step_done(psGCodeExport)) {
		// Some FFF status was invalidated, and the G-code was not exported yet.
		// Let the G-code preview UI know that the final G-code preview is not valid.
		// In addition, this early memory deallocation reduces memory footprint.
		BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": invalide gcode result %1%, will reset soon")%m_gcode_result;
		if (m_gcode_result != nullptr)
			m_gcode_result->reset();
	}
	return invalidated;
}

void BackgroundSlicingProcess::set_task(const PrintBase::TaskParams &params)
{
	assert(m_print != nullptr);
	m_print->set_task(params);
}

// Set the output path of the G-code.
void BackgroundSlicingProcess::schedule_export(const std::string &path, bool export_path_on_removable_media)
{
	assert(m_export_path.empty());
	if (! m_export_path.empty())
		return;

	// Guard against entering the export step before changing the export path.
	std::scoped_lock<std::mutex> lock(m_print->state_mutex());
	this->invalidate_step(bspsGCodeFinalize);
	m_export_path = path;
	m_export_path_on_removable_media = export_path_on_removable_media;
	// Capture preset bundle from UI thread for thread-safe access during UFP export
	m_preset_bundle = wxGetApp().preset_bundle;
}

void BackgroundSlicingProcess::schedule_upload(Slic3r::PrintHostJob upload_job)
{
	assert(m_export_path.empty());
	if (! m_export_path.empty())
		return;

	// Guard against entering the export step before changing the export path.
	std::scoped_lock<std::mutex> lock(m_print->state_mutex());
	this->invalidate_step(bspsGCodeFinalize);
	m_export_path.clear();
	m_upload_job = std::move(upload_job);
	// Capture preset bundle from UI thread for thread-safe access during UFP export
	m_preset_bundle = wxGetApp().preset_bundle;
}

void BackgroundSlicingProcess::reset_export()
{
	assert(! this->running());
	if (! this->running()) {
		m_export_path.clear();
		m_export_path_on_removable_media = false;
		// invalidate_step expects the mutex to be locked.
		std::scoped_lock<std::mutex> lock(m_print->state_mutex());
		this->invalidate_step(bspsGCodeFinalize);
	}
}

bool BackgroundSlicingProcess::set_step_started(BackgroundSlicingProcessStep step)
{
	return m_step_state.set_started(step, m_print->state_mutex(), [this](){ this->throw_if_canceled(); });
}

void BackgroundSlicingProcess::set_step_done(BackgroundSlicingProcessStep step)
{
	m_step_state.set_done(step, m_print->state_mutex(), [this](){ this->throw_if_canceled(); });
}

bool BackgroundSlicingProcess::is_step_done(BackgroundSlicingProcessStep step) const
{
	return m_step_state.is_done(step, m_print->state_mutex());
}

bool BackgroundSlicingProcess::invalidate_step(BackgroundSlicingProcessStep step)
{
    bool invalidated = m_step_state.invalidate(step, [this](){ this->stop_internal(); });
    return invalidated;
}

bool BackgroundSlicingProcess::invalidate_all_steps()
{
	return m_step_state.invalidate_all([this](){ this->stop_internal(); });
}

// Helper to export file with optional container format conversion and post-processing
// Returns true on success, false on failure with error_message populated
bool BackgroundSlicingProcess::export_to_final_path(const std::string& source_path,
                                                     const std::string& dest_path,
                                                     bool run_post_process,
                                                     std::string& error_message)
{
    BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: ENTER - source=" << source_path << ", dest=" << dest_path;
    
    // Check for nullptrs early to prevent access violations
    if (!m_fff_print) {
        BOOST_LOG_TRIVIAL(error) << "export_to_final_path: m_fff_print is NULL!";
        error_message = "Internal error: m_fff_print is null";
        return false;
    }
    
    std::string output_path = source_path;
    std::string export_path = dest_path;
    
    // Run post-processing scripts if requested
    if (run_post_process) {
        m_print->set_status(95, _u8L("Running post-processing scripts"));
        bool post_processed = run_post_process_scripts(output_path, true, "File", export_path, m_fff_print->full_print_config());
        auto remove_post_processed_temp_file = [post_processed, &output_path]() {
            if (post_processed)
                try {
                    boost::filesystem::remove(output_path);
                } catch (const std::exception& ex) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to remove temp file " << output_path << ": " << ex.what();
                }
        };
        m_print->set_status(99, _utf8(L("Successfully executed post-processing script")));
    }
    
    // Check if printer requires container format export (.ufp or .makerbot)
    std::string printer_notes = m_fff_print->full_print_config().opt_string("printer_notes");
    std::string format_type = Slic3r::FormatConfig::get_format_type_for_printer(printer_notes);
    
    // Fallback: check file extension if no format from printer_notes
    if (format_type.empty()) {
        format_type = Slic3r::FormatConfig::get_format_type_from_extension(export_path);
        if (!format_type.empty()) {
            BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: Detected container format from extension: " << format_type;
        }
    }
    
    BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: export_path=" << export_path;
    BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: format_type=" << format_type;
    
    // Container file path (created in system temp directory, deleted after successful copy)
    std::string container_path;
    
    if (!format_type.empty()) {
        // Get the appropriate file extension
        std::string ext = Slic3r::FormatConfig::get_file_extension_for_format(format_type);
        
        // Verify export_path extension matches format (warn if mismatch)
        boost::filesystem::path export_path_path(export_path);
        std::string current_ext = boost::to_lower_copy(export_path_path.extension().string());
        if (current_ext != ext) {
            BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: Extension mismatch detected. Changing from " 
                                       << current_ext << " to " << ext;
            export_path_path.replace_extension(ext);
            export_path = export_path_path.string();
        }
        
        // Create container file path in system temp directory
        boost::filesystem::path temp_path = boost::filesystem::temp_directory_path();
        temp_path /= boost::filesystem::unique_path("%%%%-%%%%-%%%%-%%%%");
        temp_path.replace_extension(ext);
        container_path = temp_path.string();
        
        BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: Building container using shared function";
        
        // Use the shared helper function for container building
        // This ensures consistent UFP generation for both export and upload paths
        if (!build_ufp_container(output_path, container_path, printer_notes, error_message)) {
            BOOST_LOG_TRIVIAL(error) << "export_to_final_path: Container build FAILED: " << error_message;
            return false;
        }
        
        BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: Container created successfully at: " << container_path;
        
        // Use container path as source for copy
        output_path = container_path;
    } else {
        BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: No container format required, exporting raw G-code";
    }
    
    // Copy file to final destination
    int copy_ret_val = CopyFileResult::SUCCESS;
    try {
        copy_ret_val = copy_file(output_path, export_path, error_message, m_export_path_on_removable_media);
    }
    catch (...) {
        // Clean up container file if it was created
        if (!container_path.empty()) {
            boost::filesystem::remove(container_path);
        }
        error_message = "Unknown error when exporting G-code.";
        return false;
    }
    
    // Clean up container file if it was created (whether copy succeeded or failed)
    if (!container_path.empty()) {
        try {
            boost::filesystem::remove(container_path);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(warning) << "export_to_final_path: Failed to clean up container file: " << e.what();
        }
    }
    
    if (copy_ret_val != CopyFileResult::SUCCESS) {
        error_message = "Failed to save G-code file.\nError message: " + error_message + ".\nSource file: " + output_path;
        return false;
    }
    
    m_print->set_status(100, GUI::format(_L("G-code file exported to %1%"), export_path));
    return true;
}

// G-code is generated in m_temp_output_path.
// Optionally run a post-processing script on a copy of m_temp_output_path.
// Copy the final G-code to target location (possibly a SD card, if it is a removable media, then verify that the file was written without an error).
void BackgroundSlicingProcess::finalize_gcode()
{
    BOOST_LOG_TRIVIAL(warning) << "finalize_gcode: ENTER - m_export_path=" << m_export_path;
    
    // Check for nullptrs early
    if (!m_fff_print) {
        BOOST_LOG_TRIVIAL(error) << "finalize_gcode: m_fff_print is NULL!";
        throw Slic3r::ExportError("Internal error: m_fff_print is null");
    }
    
    // Perform the final post-processing of the export path by applying the print statistics over the file name.
    std::string export_path = m_fff_print->print_statistics().finalize_output_path(m_export_path);
    
    BOOST_LOG_TRIVIAL(warning) << "finalize_gcode: export_path=" << export_path;
    
    std::string error_message;
    if (!export_to_final_path(m_temp_output_path, export_path, true, error_message)) {
        throw Slic3r::ExportError(error_message);
    }
    
    BOOST_LOG_TRIVIAL(warning) << "finalize_gcode: EXIT SUCCESS";
}

// G-code is generated in m_temp_output_path.
// Optionally run a post-processing script on a copy of m_temp_output_path.
// Copy the final G-code to target location (possibly a SD card, if it is a removable media, then verify that the file was written without an error).
void BackgroundSlicingProcess::export_gcode()
{
    BOOST_LOG_TRIVIAL(warning) << "export_gcode: ENTER - m_export_path=" << m_export_path;
    
    // Check for nullptrs early
    if (!m_fff_print) {
        BOOST_LOG_TRIVIAL(error) << "export_gcode: m_fff_print is NULL!";
        throw Slic3r::ExportError("Internal error: m_fff_print is null");
    }
    
    // Perform the final post-processing of the export path by applying the print statistics over the file name.
    std::string export_path = m_fff_print->print_statistics().finalize_output_path(m_export_path);
    
    BOOST_LOG_TRIVIAL(warning) << "export_gcode: export_path=" << export_path;
    
    std::string error_message;
    // Note: run_post_process=false because BBL printers have already run post-processing
    if (!export_to_final_path(m_temp_output_path, export_path, false, error_message)) {
        GUI::show_error(nullptr, wxString::FromUTF8(error_message.c_str()));
        throw Slic3r::ExportError(error_message);
    }
    
    // BBS
    auto evt = new wxCommandEvent(m_event_export_finished_id, GUI::wxGetApp().mainframe->m_plater->GetId());
    wxString output_gcode_str = wxString::FromUTF8(export_path.c_str(), export_path.length());
    evt->SetString(output_gcode_str);
    wxQueueEvent(GUI::wxGetApp().mainframe->m_plater, evt);

    // BBS: to be checked. Whether use export_path or output_path.
    gcode_add_line_number(export_path, m_fff_print->full_print_config());
}

// A print host upload job has been scheduled, enqueue it to the printhost job queue
void BackgroundSlicingProcess::prepare_upload()
{
	// Generate a unique temp path to which the gcode/zip file is copied/exported
	boost::filesystem::path source_path = boost::filesystem::temp_directory_path()
		/ boost::filesystem::unique_path("." SLIC3R_APP_KEY ".upload.%%%%-%%%%-%%%%-%%%%");

	if (m_print == m_fff_print) {
        if (m_upload_job.upload_data.use_3mf) {
            source_path = m_upload_job.upload_data.source_path;
        } else {
		    m_print->set_status(95, _utf8(L("Running post-processing scripts")));
		    std::string error_message;
		    if (copy_file(m_temp_output_path, source_path.string(), error_message) != SUCCESS)
		    	throw Slic3r::RuntimeError(_utf8(L("Copying of the temporary G-code to the output G-code failed")));
            m_upload_job.upload_data.upload_path = m_fff_print->print_statistics().finalize_output_path(m_upload_job.upload_data.upload_path.string());
		    // Orca: skip post-processing scripts for BBL printers as we have run them already in finalize_gcode()
		    // todo: do we need to copy the file?
		
            // Check if container format conversion is needed for print host upload
            // (e.g., UltiMaker LAN requires .ufp format)
            std::string printer_notes = m_fff_print->full_print_config().opt_string("printer_notes");
            std::string format_type = Slic3r::FormatConfig::get_format_type_for_printer(printer_notes);
            
            if (!format_type.empty()) {
                // Need to convert to container format
                std::string container_ext = Slic3r::FormatConfig::get_file_extension_for_format(format_type);
                boost::filesystem::path container_path = source_path;
                container_path.replace_extension(container_ext);
                
                BOOST_LOG_TRIVIAL(warning) << "prepare_upload: Building container using shared function";
                
                // Use the shared helper function for container building
                // This ensures consistent UFP generation for both export and upload paths
                if (!build_ufp_container(source_path.string(), container_path.string(), printer_notes, error_message)) {
                    BOOST_LOG_TRIVIAL(error) << "prepare_upload: Container build FAILED: " << error_message;
                    throw Slic3r::RuntimeError("Failed to build container format: " + error_message);
                }
                
                // Remove the original source file and use container
                try {
                    boost::filesystem::remove(source_path);
                } catch (...) {}
                source_path = container_path;
                
                // Update upload path extension to match container
                m_upload_job.upload_data.upload_path.replace_extension(container_ext);
                
                BOOST_LOG_TRIVIAL(info) << "prepare_upload: Converted to container format: " << container_path.string();
            }
            
            if (!m_fff_print->is_BBL_printer()) {
                std::string source_path_str = source_path.string();
                std::string output_name_str = m_upload_job.upload_data.upload_path.string();
                if (run_post_process_scripts(source_path_str, false, m_upload_job.printhost->get_name(), output_name_str,
                                             m_fff_print->full_print_config()))
			    m_upload_job.upload_data.upload_path = output_name_str;
			}
		}
    } else {
        m_upload_job.upload_data.upload_path = m_sla_print->print_statistics().finalize_output_path(m_upload_job.upload_data.upload_path.string());
        
        ThumbnailsList thumbnails = this->render_thumbnails(
        	ThumbnailsParams{current_print()->full_print_config().option<ConfigOptionPoints>("thumbnails")->values, true, true, true, true});
																												 // true, false, true, true); // renders also supports and pad
        Zipper zipper{source_path.string()};
        m_sla_archive.export_print(zipper, *m_sla_print, m_upload_job.upload_data.upload_path.string());
        for (const ThumbnailData& data : thumbnails)
	        if (data.is_valid())
	            write_thumbnail(zipper, data);
        zipper.finalize();
    }

    m_print->set_status(100, (boost::format(_utf8(L("Scheduling upload to `%1%`. See Window -> Print Host Upload Queue"))) % m_upload_job.printhost->get_host()).str());

	m_upload_job.upload_data.source_path = std::move(source_path);

	GUI::wxGetApp().printhost_job_queue().enqueue(std::move(m_upload_job));
}
// Executed by the background thread, to start a task on the UI thread.
ThumbnailsList BackgroundSlicingProcess::render_thumbnails(const ThumbnailsParams &params)
{
	ThumbnailsList thumbnails;
	if (m_thumbnail_cb)
		this->execute_ui_task([this, &params, &thumbnails](){ thumbnails = m_thumbnail_cb(params); });
	return thumbnails;
}

}; // namespace Slic3r
