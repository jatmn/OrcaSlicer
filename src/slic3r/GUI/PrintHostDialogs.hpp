#ifndef slic3r_PrintHostSendDialog_hpp_
#define slic3r_PrintHostSendDialog_hpp_

#include <set>
#include <string>
#include <functional>
#include <boost/filesystem/path.hpp>

#include <wx/string.h>
#include <wx/event.h>
#include <wx/dialog.h>

#include "GUI_Utils.hpp"
#include "MsgDialog.hpp"
#include "../Utils/PrintHost.hpp"
#include "libslic3r/PrintConfig.hpp"

class wxButton;
class wxTextCtrl;
class wxChoice;
class wxComboBox;
class wxDataViewListCtrl;

namespace Slic3r {

namespace GUI {

class PrintHostSendDialog : public GUI::MsgDialog
{
public:
    PrintHostSendDialog(const boost::filesystem::path &path, PrintHostPostUploadActions post_actions, const wxArrayString& groups, const wxArrayString& storage_paths, const wxArrayString& storage_names, bool switch_to_device_tab);
    PrintHostSendDialog(const boost::filesystem::path &path, PrintHostPostUploadActions post_actions, const wxArrayString& groups, const wxArrayString& storage_paths, const wxArrayString& storage_names, bool switch_to_device_tab,
                       const wxArrayString& project_names, const wxArrayString& project_ids, const std::string& last_project_id = "", bool no_projects = false);
    // Constructor with host_type parameter to control button visibility
    PrintHostSendDialog(const boost::filesystem::path &path, PrintHostPostUploadActions post_actions, const wxArrayString& groups, const wxArrayString& storage_paths, const wxArrayString& storage_names, bool switch_to_device_tab,
                       const PrintHostType* host_type);
    virtual ~PrintHostSendDialog() {}
    boost::filesystem::path filename() const;
    PrintHostPostUploadAction post_action() const;
    std::string group() const;
    std::string storage() const;
    bool switch_to_device_tab() const {return m_switch_to_device_tab;}

    virtual void EndModal(int ret) override;
    virtual void init();
    virtual std::map<std::string, std::string> extendedInfo() const;

protected:
    wxTextCtrl *txt_filename;
    wxComboBox *combo_groups;
    wxComboBox* combo_storage;
    // UltiMaker Digital Factory project/folder selection
    wxComboBox* combo_projects;
    wxButton* btn_new_project;
    wxButton* btn_refresh_projects;
    PrintHostPostUploadAction post_upload_action;
    wxString    m_valid_suffix;
    wxString    m_preselected_storage;
    wxArrayString m_paths;
    bool m_switch_to_device_tab;

    boost::filesystem::path m_path;
    PrintHostPostUploadActions m_post_actions;
    wxArrayString m_storage_names;
    // UltiMaker Digital Factory project/folder data
    wxArrayString m_project_names;
    wxArrayString m_project_ids;
    // Last selected project ID (for remembering user preference)
    std::string m_last_project_id;
    // Pending new project name (used when user clicks "+" button)
    std::string m_pending_new_project_name;
    // Callback for creating a new project on the server
    std::function<PrintHost::CreateProjectResult(const std::string& name)> m_project_create_callback;
    // Callback for refreshing the project list from server
    std::function<bool(wxArrayString&, wxArrayString&)> m_project_refresh_callback;
    // Message label shown when no projects exist
    wxStaticText* m_project_msg_label;
    // Flag indicating if we started with no projects
    bool m_no_projects;
    // Cached button pointers for enabling after project creation
    Button* m_btn_upload;
    Button* m_btn_upload_and_print;
    // Host type to control button visibility (e.g., hide Upload button for UltiMaker LAN)
    const PrintHostType* m_host_type;

public:
    void add_project(const wxString& name, const wxString& id);
    std::string get_pending_new_project_name() const { return m_pending_new_project_name; }
    void clear_pending_new_project_name() { m_pending_new_project_name.clear(); }
    void set_project_create_callback(std::function<PrintHost::CreateProjectResult(const std::string&)> callback) { m_project_create_callback = callback; }
    void set_project_refresh_callback(std::function<bool(wxArrayString&, wxArrayString&)> callback) { m_project_refresh_callback = callback; }
};


class PrintHostQueueDialog : public DPIDialog
{
public:
    class Event : public wxEvent
    {
    public:
        size_t job_id;
        int progress = 0;    // in percent
        wxString tag;
        wxString status;

        Event(wxEventType eventType, int winid, size_t job_id);
        Event(wxEventType eventType, int winid, size_t job_id, int progress);
        Event(wxEventType eventType, int winid, size_t job_id, wxString error);
        Event(wxEventType eventType, int winid, size_t job_id, wxString tag, wxString status);

        virtual wxEvent *Clone() const;
    };


    PrintHostQueueDialog(wxWindow *parent);

    void append_job(const PrintHostJob &job);
    void get_active_jobs(std::vector<std::pair<std::string, std::string>>& ret);

    virtual bool Show(bool show = true) override
    {
        if(!show)
            save_user_data(UDT_SIZE | UDT_POSITION | UDT_COLS);
        return DPIDialog::Show(show);
    }
protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    void on_sys_color_changed() override;

private:
    enum Column {
        COL_ID,
        COL_PROGRESS,
        COL_STATUS,
        COL_HOST,
        COL_SIZE,
        COL_FILENAME,
        COL_ERRORMSG
    };

    enum JobState {
        ST_NEW,
        ST_PROGRESS,
        ST_ERROR,
        ST_CANCELLING,
        ST_CANCELLED,
        ST_COMPLETED,
    };

    enum { HEIGHT = 60, WIDTH = 30, SPACING = 5 };

    enum UserDataType{
        UDT_SIZE = 1,
        UDT_POSITION = 2,
        UDT_COLS = 4
    };

    wxButton *btn_cancel;
    wxButton *btn_error;
    wxDataViewListCtrl *job_list;
    // Note: EventGuard prevents delivery of progress evts to a freed PrintHostQueueDialog
    EventGuard on_progress_evt;
    EventGuard on_error_evt;
    EventGuard on_cancel_evt;
    EventGuard on_info_evt;

    JobState get_state(int idx);
    void set_state(int idx, JobState);
    void on_list_select();
    void on_progress(Event&);
    void on_error(Event&);
    void on_cancel(Event&);
    void on_info(Event&);
    // This vector keep adress and filename of uploads. It is used when checking for running uploads during exit.
    std::vector<std::pair<std::string, std::string>> upload_names;
    void save_user_data(int);
    bool load_user_data(int, std::vector<int>&);
};

class ElegooPrintHostSendDialog : public PrintHostSendDialog
{
public:
    ElegooPrintHostSendDialog(const boost::filesystem::path& path,
                              PrintHostPostUploadActions     post_actions,
                              const wxArrayString&           groups,
                              const wxArrayString&           storage_paths,
                              const wxArrayString&           storage_names,
                              bool                           switch_to_device_tab);

    virtual void EndModal(int ret) override;
    int          timeLapse() const { return m_timeLapse; }
    int          heatedBedLeveling() const { return m_heatedBedLeveling; }
    BedType      bedType() const { return m_BedType; }

    virtual void                               init() override;
    virtual std::map<std::string, std::string> extendedInfo() const
    {
        return {{"bedType", std::to_string(static_cast<int>(m_BedType))},
                {"timeLapse", std::to_string(m_timeLapse)},
                {"heatedBedLeveling", std::to_string(m_heatedBedLeveling)}};
    }

private:
    BedType appBedType() const;
    void    refresh();

    const char* CONFIG_KEY_UPLOADANDPRINT    = "elegoolink_upload_and_print";
    const char* CONFIG_KEY_TIMELAPSE         = "elegoolink_timelapse";
    const char* CONFIG_KEY_HEATEDBEDLEVELING = "elegoolink_heated_bed_leveling";
    const char* CONFIG_KEY_BEDTYPE           = "elegoolink_bed_type";

private:
    wxStaticText* warning_text{nullptr};
    wxBoxSizer*   uploadandprint_sizer{nullptr};

    int     m_timeLapse;
    int     m_heatedBedLeveling;
    BedType m_BedType;
};

wxDECLARE_EVENT(EVT_PRINTHOST_PROGRESS, PrintHostQueueDialog::Event);
wxDECLARE_EVENT(EVT_PRINTHOST_ERROR, PrintHostQueueDialog::Event);
wxDECLARE_EVENT(EVT_PRINTHOST_CANCEL, PrintHostQueueDialog::Event);
wxDECLARE_EVENT(EVT_PRINTHOST_INFO, PrintHostQueueDialog::Event);
}}

#endif
