#include "slic3r/Utils/Bonjour.hpp"   // On Windows, boost needs to be included before wxWidgets headers

#include "UltiMakerDialog.hpp"

#include <set>
#include <mutex>

#include <boost/nowide/convert.hpp>
#include <boost/log/trivial.hpp>

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/wupdlock.h>

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/Utils/Bonjour.hpp"

namespace Slic3r {

namespace {

constexpr unsigned DISCOVERY_PASS_TIMEOUT_SECONDS = 2;
constexpr unsigned DISCOVERY_PASS_RETRIES         = 1;

}

class BonjourReplyEvent : public wxEvent
{
public:
	BonjourReply reply;

	BonjourReplyEvent(wxEventType eventType, int winid, BonjourReply &&reply) :
		wxEvent(winid, eventType),
		reply(std::move(reply))
	{}

	virtual wxEvent *Clone() const
	{
		return new BonjourReplyEvent(*this);
	}
};

wxDEFINE_EVENT(EVT_ULTIMAKER_REPLY, BonjourReplyEvent);

wxDECLARE_EVENT(EVT_ULTIMAKER_COMPLETE, wxCommandEvent);
wxDEFINE_EVENT(EVT_ULTIMAKER_COMPLETE, wxCommandEvent);

class ReplySet: public std::set<BonjourReply> {};

struct LifetimeGuard
{
	std::mutex mutex;
	UltiMakerDialog *dialog;

	LifetimeGuard(UltiMakerDialog *dialog) : dialog(dialog) {}
};

UltiMakerDialog::UltiMakerDialog(wxWindow *parent, Slic3r::PrinterTechnology tech)
	: wxDialog(parent, wxID_ANY, _(L("UltiMaker Network Discovery")), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
	, list(new wxListView(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxSIMPLE_BORDER))
	, replies(new ReplySet)
	, label(new wxStaticText(this, wxID_ANY, ""))
	, timer(new wxTimer())
	, timer_state(0)
	, tech(tech)
{
	const int em = GUI::wxGetApp().em_unit();
	list->SetMinSize(wxSize(80 * em, 30 * em));

	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);

	vsizer->Add(label, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, em);

	list->SetSingleStyle(wxLC_SINGLE_SEL);
	list->SetSingleStyle(wxLC_SORT_DESCENDING);
	list->AppendColumn(_(L("Address")), wxLIST_FORMAT_LEFT, 5 * em);
	list->AppendColumn(_(L("Hostname")), wxLIST_FORMAT_LEFT, 10 * em);
	list->AppendColumn(_(L("Service name")), wxLIST_FORMAT_LEFT, 20 * em);
	if (tech == ptFFF) {
		list->AppendColumn(_(L("Model")), wxLIST_FORMAT_LEFT, 10 * em);
		list->AppendColumn(_(L("Firmware")), wxLIST_FORMAT_LEFT, 8 * em);
	}

	vsizer->Add(list, 1, wxEXPAND | wxALL, em);

	wxBoxSizer *button_sizer = new wxBoxSizer(wxHORIZONTAL);
	button_sizer->Add(new wxButton(this, wxID_OK, _L("OK")), 0, wxALL, em);
	button_sizer->Add(new wxButton(this, wxID_CANCEL, _L("Cancel")), 0, wxALL, em);
	// ^ Note: The Ok/Cancel labels are translated by wxWidgets

	vsizer->Add(button_sizer, 0, wxALIGN_CENTER);
	SetSizerAndFit(vsizer);

	Bind(EVT_ULTIMAKER_REPLY, &UltiMakerDialog::on_reply, this);

	Bind(EVT_ULTIMAKER_COMPLETE, [this](wxCommandEvent &) {
		if (this->discovery_active) {
			this->start_lookup();
		} else {
			this->timer_state = 0;
		}
	});

	Bind(wxEVT_TIMER, &UltiMakerDialog::on_timer, this);
	GUI::wxGetApp().UpdateDlgDarkUI(this);
}

UltiMakerDialog::~UltiMakerDialog()
{
	// Needed because of forward defs
}

bool UltiMakerDialog::show_and_lookup()
{
	Show();   // Because we need GetId() to work before ShowModal()

	timer->Stop();
	timer->SetOwner(this);
	discovery_active = true;
	timer_state = 1;
	timer->Start(1000);
    on_timer_process();

	// The background thread needs to queue messages for this dialog
	// and for that it needs a valid pointer to it (mandated by the wxWidgets API).
	// Here we put the pointer under a shared_ptr and protect it by a mutex,
	// so that both threads can access it safely.
	lifetime_guard = std::make_shared<LifetimeGuard>(this);
	start_lookup();

	bool res = ShowModal() == wxID_OK && list->GetFirstSelected() >= 0;
	{
		discovery_active = false;
		bonjour.reset();
		timer->Stop();
		// Tell the background thread the dialog is going away...
		std::lock_guard<std::mutex> lock_guard(lifetime_guard->mutex);
		lifetime_guard->dialog = nullptr;
	}
	lifetime_guard.reset();
	return res;
}

wxString UltiMakerDialog::get_selected() const
{
	auto sel = list->GetFirstSelected();
	return sel >= 0 ? list->GetItemText(sel) : wxString();
}

void UltiMakerDialog::start_lookup()
{
	if (!discovery_active || !lifetime_guard) {
		return;
	}

	// Cura keeps network discovery running in the background. Orca's Bonjour lookup
	// is request/timeout based, so keep the dialog persistent by continuously running
	// short discovery passes while the picker remains open.
	Bonjour::TxtKeys txt_keys { "type", "model", "version", "firmware" };

	bonjour = Bonjour("_ultimaker._tcp.local.")
		.set_txt_keys(std::move(txt_keys))
		.set_retries(DISCOVERY_PASS_RETRIES)
		.set_timeout(DISCOVERY_PASS_TIMEOUT_SECONDS)
		.on_reply([guard = lifetime_guard](BonjourReply &&reply) {
			std::lock_guard<std::mutex> lock_guard(guard->mutex);
			auto dialog = guard->dialog;
			if (dialog != nullptr) {
				auto evt = new BonjourReplyEvent(EVT_ULTIMAKER_REPLY, dialog->GetId(), std::move(reply));
				wxQueueEvent(dialog, evt);
			}
		})
		.on_complete([guard = lifetime_guard]() {
			std::lock_guard<std::mutex> lock_guard(guard->mutex);
			auto dialog = guard->dialog;
			if (dialog != nullptr) {
				auto evt = new wxCommandEvent(EVT_ULTIMAKER_COMPLETE, dialog->GetId());
				wxQueueEvent(dialog, evt);
			}
		})
		.lookup();
}


// Private

void UltiMakerDialog::on_reply(BonjourReplyEvent &e)
{
	// Filter by type="printer" as Cura does
	const auto type = e.reply.txt_data.find("type");
	if (type == e.reply.txt_data.end() || type->second != "printer") {
		// Not an UltiMaker printer, skip
		return;
	}

	if (replies->find(e.reply) != replies->end()) {
		// We already have this reply
		return;
	}

	// Filter replies based on selected technology
	// UltiMaker printers are FFF only, but we keep the tech check for consistency
	if (tech == ptSLA) {
		// UltiMaker doesn't have SLA printers, but we could still show them
		// return;
	}

    replies->insert(std::move(e.reply));

	auto selected = get_selected();

	wxWindowUpdateLocker freeze_guard(this);
	(void)freeze_guard;

	list->DeleteAllItems();

	// The whole list is recreated so that we benefit from it already being sorted in the set.
	// (And also because wxListView's sorting API is bananas.)
	for (const auto &reply : *replies) {
		auto item = list->InsertItem(0, reply.ip.to_string());
		list->SetItem(item, 1, reply.hostname);
		list->SetItem(item, 2, reply.service_name);

		if (tech == ptFFF) {
			// Display model if available
			const auto model_it = reply.txt_data.find("model");
			if (model_it != reply.txt_data.end()) {
				list->SetItem(item, 3, GUI::from_u8(model_it->second));
			}

			// Display firmware version if available
			const auto firmware_it = reply.txt_data.find("firmware");
			if (firmware_it != reply.txt_data.end()) {
				list->SetItem(item, 4, GUI::from_u8(firmware_it->second));
			} else {
				// Fallback to version
				const auto version_it = reply.txt_data.find("version");
				if (version_it != reply.txt_data.end()) {
					list->SetItem(item, 4, GUI::from_u8(version_it->second));
				}
			}
		}
	}

	const int em = GUI::wxGetApp().em_unit();

	for (int i = 0; i < list->GetColumnCount(); i++) {
		list->SetColumnWidth(i, wxLIST_AUTOSIZE);
		if (list->GetColumnWidth(i) < 10 * em) { list->SetColumnWidth(i, 10 * em); }
	}

	if (!selected.IsEmpty()) {
		// Attempt to preserve selection
		auto hit = list->FindItem(-1, selected);
		if (hit >= 0) { list->SetItemState(hit, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED); }
	}
}

void UltiMakerDialog::on_timer(wxTimerEvent &)
{
    on_timer_process();
}

// This is here so the function can be bound to wxEVT_TIMER and also called
// explicitly (wxTimerEvent should not be created by user code).
void UltiMakerDialog::on_timer_process()
{
    const auto search_str = _L("Searching for UltiMaker printers");

    if (discovery_active && timer_state > 0) {
        const std::string dots(timer_state, '.');
        label->SetLabel(search_str + dots);
        timer_state = (timer_state) % 3 + 1;
    } else {
        label->SetLabel(search_str + ": " + _L("Finished") + ".");
        timer->Stop();
    }
}

}
