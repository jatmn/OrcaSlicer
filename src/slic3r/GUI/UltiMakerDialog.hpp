#ifndef slic3r_UltiMakerDialog_hpp_
#define slic3r_UltiMakerDialog_hpp_

#include <cstddef>
#include <memory>

#include <boost/asio/ip/address.hpp>

#include <wx/dialog.h>
#include <wx/string.h>

#include "libslic3r/PrintConfig.hpp"

class wxListView;
class wxStaticText;
class wxTimer;
class wxTimerEvent;
class address;

namespace Slic3r {

class Bonjour;
class BonjourReplyEvent;
class ReplySet;


class UltiMakerDialog: public wxDialog
{
public:
	UltiMakerDialog(wxWindow *parent, Slic3r::PrinterTechnology);
	UltiMakerDialog(UltiMakerDialog &&) = delete;
	UltiMakerDialog(const UltiMakerDialog &) = delete;
	UltiMakerDialog &operator=(UltiMakerDialog &&) = delete;
	UltiMakerDialog &operator=(const UltiMakerDialog &) = delete;
	~UltiMakerDialog();

	bool show_and_lookup();
	wxString get_selected() const;
private:
	wxListView *list;
	std::unique_ptr<ReplySet> replies;
	wxStaticText *label;
	std::shared_ptr<Bonjour> bonjour;
	std::unique_ptr<wxTimer> timer;
	unsigned timer_state;
	Slic3r::PrinterTechnology tech;

	virtual void on_reply(BonjourReplyEvent &);
	void on_timer(wxTimerEvent &);
    void on_timer_process();
};

}

#endif
