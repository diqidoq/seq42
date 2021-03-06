//----------------------------------------------------------------------------
//
//  This file is part of seq42.
//
//  seq42 is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  seq42 is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with seq42; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------

#include <cctype>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <gtk/gtkversion.h>

#include "mainwnd.h"
#include "perform.h"
#include "midifile.h"
#include "sequence.h"
#include "font.h"
#include "seqlist.h"

#include "pixmaps/seq42_32.xpm"
#include "pixmaps/play2.xpm"
#include "pixmaps/seqlist.xpm"
#include "pixmaps/stop.xpm"
#include "pixmaps/snap.xpm"
#include "pixmaps/expand.xpm"
#include "pixmaps/collapse.xpm"
#include "pixmaps/loop.xpm"
#include "pixmaps/copy.xpm"
#include "pixmaps/undo.xpm"
#include "pixmaps/redo.xpm"
#include "pixmaps/down.xpm"
#include "pixmaps/transportFollow.xpm"
#include "pixmaps/transpose.xpm"
#include "pixmaps/fastforward.xpm"
#include "pixmaps/rewind.xpm"

#ifdef JACK_SUPPORT
#include "pixmaps/jack.xpm"
#endif // JACK_SUPPORT

using namespace sigc;

short global_file_int_size = sizeof(int32_t);
short global_file_long_int_size = sizeof(int32_t);

bool global_is_running = false;
bool global_is_modified = false;
bool global_seqlist_need_update = false;
ff_rw_type_e FF_RW_button_type = FF_RW_RELEASE;

// tooltip helper, for old vs new gtk...
#if GTK_MINOR_VERSION >= 12
#   define add_tooltip( obj, text ) obj->set_tooltip_text( text);
#else
#   define add_tooltip( obj, text ) m_tooltips->set_tip( *obj, text );
#endif



mainwnd::mainwnd(perform *a_p):
    m_mainperf(a_p),
    m_options(NULL),
    m_snap(c_ppqn / 4),
    m_bp_measure(4),
    m_bw(4)
{
    using namespace Menu_Helpers;

    set_icon(Gdk::Pixbuf::create_from_xpm_data(seq42_32_xpm));

    /* main window */
    update_window_title();
    set_size_request(860, 322);

#if GTK_MINOR_VERSION < 12
    m_tooltips = manage( new Tooltips() );
#endif
    m_main_time = manage( new maintime( ));

    m_menubar = manage(new MenuBar());

    m_menu_file = manage(new Menu());
    m_menubar->items().push_front(MenuElem("_File", *m_menu_file));
    
    m_menu_recent = nullptr;

    m_menu_edit = manage(new Menu());
    m_menubar->items().push_back(MenuElem("_Edit", *m_menu_edit));

    m_menu_help = manage( new Menu());
    m_menubar->items().push_back(MenuElem("_Help", *m_menu_help));

    /* file menu items */
    m_menu_file->items().push_back(MenuElem("_New",
                                            Gtk::AccelKey("<control>N"),
                                            mem_fun(*this, &mainwnd::file_new)));
    m_menu_file->items().push_back(MenuElem("_Open...",
                                            Gtk::AccelKey("<control>O"),
                                            mem_fun(*this, &mainwnd::file_open)));
    update_recent_files_menu();
    
    m_menu_file->items().push_back(MenuElem("Open _setlist...",
                                            mem_fun(*this, &mainwnd::file_open_setlist)));
    
    m_menu_file->items().push_back(SeparatorElem());
    
    m_menu_file->items().push_back(MenuElem("_Save",
                                            Gtk::AccelKey("<control>S"),
                                            mem_fun(*this, &mainwnd::file_save)));
    m_menu_file->items().push_back(MenuElem("Save _as...",
                                            sigc::bind(mem_fun(*this, &mainwnd::file_save_as), E_SEQ42_NATIVE_FILE, nullptr)));

    m_menu_file->items().push_back(SeparatorElem());

    m_menu_file->items().push_back(MenuElem("O_ptions...",
                                            mem_fun(*this,&mainwnd::options_dialog)));
    m_menu_file->items().push_back(SeparatorElem());
    m_menu_file->items().push_back(MenuElem("E_xit",
                                            Gtk::AccelKey("<control>Q"),
                                            mem_fun(*this, &mainwnd::file_exit)));

    /* edit menu items */
    m_menu_edit->items().push_back(MenuElem("Sequence _list",
                                            mem_fun(*this, &mainwnd::open_seqlist)));

    m_menu_edit->items().push_back(MenuElem("_Apply song transpose",
                                            mem_fun(*this, &mainwnd::apply_song_transpose)));

    m_menu_edit->items().push_back(MenuElem("Increase _grid size",
                                            mem_fun(*this, &mainwnd::grow)));

    m_menu_edit->items().push_back(MenuElem("_Delete unused sequences",
                                            mem_fun(*this, &mainwnd::delete_unused_seq)));

    m_menu_edit->items().push_back(MenuElem("_Create triggers between L and R for 'playing' sequences",
                                            mem_fun(*this, &mainwnd::create_triggers)));

    m_menu_edit->items().push_back(SeparatorElem());
    m_menu_edit->items().push_back(MenuElem("_Mute all tracks",
                                            sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_ON)));

    m_menu_edit->items().push_back(MenuElem("_Unmute all tracks",
                                            sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_OFF)));

    m_menu_edit->items().push_back(MenuElem("_Toggle mute for all tracks",
                                            sigc::bind(mem_fun(*this, &mainwnd::set_song_mute), MUTE_TOGGLE)));

    m_menu_edit->items().push_back(SeparatorElem());
    m_menu_edit->items().push_back(MenuElem("_Import midi...",
                                            mem_fun(*this, &mainwnd::file_import_dialog)));

    m_menu_edit->items().push_back(MenuElem("Midi e_xport (Seq 24/32/64)",
                                            sigc::bind(mem_fun(*this, &mainwnd::file_save_as), E_MIDI_SEQ24_FORMAT, nullptr)));

    m_menu_edit->items().push_back(MenuElem("Midi export _song",
                                            sigc::bind(mem_fun(*this, &mainwnd::file_save_as), E_MIDI_SONG_FORMAT, nullptr)));

    /* help menu items */
    m_menu_help->items().push_back(MenuElem("_About...",
                                            mem_fun(*this, &mainwnd::about_dialog)));

    /* top line items */
    HBox *hbox1 = manage( new HBox( false, 2 ) );
    hbox1->set_border_width( 2 );

    m_button_stop = manage( new Button() );
    m_button_stop->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( stop_xpm ))));
    m_button_stop->signal_clicked().connect( mem_fun( *this, &mainwnd::stop_playing));
    add_tooltip( m_button_stop, "Stop playing." );

    m_button_rewind = manage( new Button() );
    m_button_rewind->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( rewind_xpm ))));
    m_button_rewind->signal_pressed().connect(sigc::bind (mem_fun( *this, &mainwnd::rewind), true));
    m_button_rewind->signal_released().connect(sigc::bind (mem_fun( *this, &mainwnd::rewind), false));
    add_tooltip( m_button_rewind, "Rewind." );

    m_button_play = manage( new Button() );
    m_button_play->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( play2_xpm ))));
    m_button_play->signal_clicked().connect(  mem_fun( *this, &mainwnd::start_playing));
    add_tooltip( m_button_play, "Begin playing at L marker." );

    m_button_fastforward = manage( new Button() );
    m_button_fastforward->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( fastforward_xpm ))));
    m_button_fastforward->signal_pressed().connect(sigc::bind (mem_fun( *this, &mainwnd::fast_forward), true));
    m_button_fastforward->signal_released().connect(sigc::bind (mem_fun( *this, &mainwnd::fast_forward), false));
    add_tooltip( m_button_fastforward, "Fast Forward." );

    m_button_loop = manage( new ToggleButton() );
    m_button_loop->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( loop_xpm ))));
    m_button_loop->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_looped ));
    add_tooltip( m_button_loop, "Play looped between L and R." );

    m_button_mode = manage( new ToggleButton( "Song" ) );
    m_button_mode->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_song_mode ));
    add_tooltip( m_button_mode, "Toggle song mode (or live/sequence mode)." );
    if(global_song_start_mode)
    {
        m_button_mode->set_active( true );
    }

#ifdef JACK_SUPPORT
    m_button_jack = manage( new ToggleButton() );
    m_button_jack->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( jack_xpm ))));
    m_button_jack->signal_toggled().connect(  mem_fun( *this, &mainwnd::set_jack_mode ));
    add_tooltip( m_button_jack, "Toggle Jack sync connection" );
    if(global_with_jack_transport)
    {
        m_button_jack->set_active( true );
    }
#endif

    m_button_seq = manage( new Button() );
    m_button_seq->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( seqlist_xpm ))));
    m_button_seq->signal_clicked().connect(  mem_fun( *this, &mainwnd::open_seqlist ));
    add_tooltip( m_button_seq, "Open sequence list" );

    m_button_follow = manage( new ToggleButton() );
    m_button_follow->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( transportFollow_xpm ))));
    m_button_follow->signal_clicked().connect(  mem_fun( *this, &mainwnd::set_follow_transport ));
    add_tooltip( m_button_follow, "Follow transport" );
    m_button_follow->set_active(true);

    hbox1->pack_start( *m_button_stop, false, false );
    hbox1->pack_start( *m_button_rewind, false, false );
    hbox1->pack_start( *m_button_play, false, false );
    hbox1->pack_start( *m_button_fastforward, false, false );
    hbox1->pack_start( *m_button_loop, false, false );
    hbox1->pack_start( *m_button_mode, false, false );
#ifdef JACK_SUPPORT
    hbox1->pack_start(*m_button_jack, false, false );
#endif
    hbox1->pack_start( *m_button_seq, false, false );
    hbox1->pack_start( *m_button_follow, false, false );

    // adjust placement...
    VBox *vbox_b = manage( new VBox() );
    HBox *hbox2 = manage( new HBox( false, 0 ) );
    vbox_b->pack_start( *hbox2, false, false );
    hbox1->pack_end( *vbox_b, false, false );
    hbox2->set_spacing( 10 );

    /* timeline */
    hbox2->pack_start( *m_main_time, false, false );

    /* perfedit widgets */
    m_vadjust = manage( new Adjustment(0,0,1,1,1,1 ));
    m_hadjust = manage( new Adjustment(0,0,1,1,1,1 ));

    m_vscroll   =  manage(new VScrollbar( *m_vadjust ));
    m_hscroll   =  manage(new HScrollbar( *m_hadjust ));

    m_perfnames = manage( new perfnames( m_mainperf, this, m_vadjust ));

    m_perfroll = manage( new perfroll
                         (
                             m_mainperf,
                             this,
                             m_hadjust,
                             m_vadjust
                         ));
    m_perftime = manage( new perftime( m_mainperf, this, m_hadjust ));
    m_tempo = manage( new tempo( m_mainperf, this, m_hadjust ));
    
    /* init table, viewports and scroll bars */
    m_table     = manage( new Table( 6, 3, false));
    m_table->set_border_width( 2 );

    m_hbox      = manage( new HBox( false, 2 ));
    m_hlbox     = manage( new HBox( false, 2 ));

    m_hlbox->set_border_width( 2 );

    /* fill table */
    m_table->attach( *m_hlbox,  0, 3, 0, 1,  Gtk::FILL, Gtk::SHRINK, 2, 0 ); // shrink was 0

    m_table->attach( *m_perfnames,    0, 1, 3, 4, Gtk::SHRINK, Gtk::FILL );
    m_table->attach( *m_tempo, 1, 2, 1, 2, Gtk::FILL, Gtk::SHRINK );
    
    Label* tempolabel = manage(new Label("TEMPO",0, Gtk::ALIGN_END)); // FIXME
    m_table->attach( *tempolabel,0,1,1,2, Gtk::SHRINK, Gtk::SHRINK);

    m_table->attach( *m_perftime, 1, 2, 2, 3, Gtk::FILL, Gtk::SHRINK );
    m_table->attach( *m_perfroll, 1, 2, 3, 4,
                     Gtk::FILL | Gtk::SHRINK,
                     Gtk::FILL | Gtk::SHRINK );

    m_table->attach( *m_vscroll, 2, 3, 3, 4, Gtk::SHRINK, Gtk::FILL | Gtk::EXPAND  );

    m_table->attach( *m_hbox,  0, 1, 4, 5,  Gtk::FILL, Gtk::SHRINK, 0, 2 );
    m_table->attach( *m_hscroll, 1, 2, 4, 5, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK  );

    m_menu_xpose =   manage( new Menu());
    char num[11];
    for ( int i=-12; i<=12; ++i)
    {
        if (i)
        {
            snprintf(num, sizeof(num), "%+d [%s]", i, c_interval_text[abs(i)]);
        }
        else
        {
            snprintf(num, sizeof(num), "0 [normal]");
        }
        m_menu_xpose->items().push_front( MenuElem( num,
                                          sigc::bind(mem_fun(*this,&mainwnd::xpose_button_callback),
                                                  i )));
    }

    m_button_xpose = manage( new Button());
    m_button_xpose->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( transpose_xpm ))));
    m_button_xpose->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_xpose  ));
    add_tooltip( m_button_xpose, "Song transpose" );
    m_entry_xpose = manage( new Entry());
    m_entry_xpose->set_size_request( 40, -1 );
    m_entry_xpose->set_editable( false );

    m_menu_snap =   manage( new Menu());
    m_menu_snap->items().push_back(MenuElem("1/1",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 1  )));
    m_menu_snap->items().push_back(MenuElem("1/2",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 2  )));
    m_menu_snap->items().push_back(MenuElem("1/4",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 4  )));
    m_menu_snap->items().push_back(MenuElem("1/8",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 8  )));
    m_menu_snap->items().push_back(MenuElem("1/16",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 16  )));
    m_menu_snap->items().push_back(MenuElem("1/32",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 32  )));
    m_menu_snap->items().push_back(SeparatorElem());
    m_menu_snap->items().push_back(MenuElem("1/3",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 3  )));
    m_menu_snap->items().push_back(MenuElem("1/6",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 6  )));
    m_menu_snap->items().push_back(MenuElem("1/12",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 12  )));
    m_menu_snap->items().push_back(MenuElem("1/24",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 24  )));
    m_menu_snap->items().push_back(SeparatorElem());
    m_menu_snap->items().push_back(MenuElem("1/5",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 5  )));
    m_menu_snap->items().push_back(MenuElem("1/10",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 10  )));
    m_menu_snap->items().push_back(MenuElem("1/20",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 20  )));
    m_menu_snap->items().push_back(MenuElem("1/40",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 40  )));
    m_menu_snap->items().push_back(SeparatorElem());
    m_menu_snap->items().push_back(MenuElem("1/7",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 7  )));
    m_menu_snap->items().push_back(MenuElem("1/9",   sigc::bind(mem_fun(*this,&mainwnd::set_snap), 9  )));
    m_menu_snap->items().push_back(MenuElem("1/11",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 11  )));
    m_menu_snap->items().push_back(MenuElem("1/13",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 13  )));
    m_menu_snap->items().push_back(MenuElem("1/14",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 14  )));
    m_menu_snap->items().push_back(MenuElem("1/15",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 15  )));
    m_menu_snap->items().push_back(MenuElem("1/18",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 18  )));
    m_menu_snap->items().push_back(MenuElem("1/22",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 22  )));
    m_menu_snap->items().push_back(MenuElem("1/26",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 26  )));
    m_menu_snap->items().push_back(MenuElem("1/28",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 28  )));
    m_menu_snap->items().push_back(MenuElem("1/30",  sigc::bind(mem_fun(*this,&mainwnd::set_snap), 30  )));

    /* snap */
    m_button_snap = manage( new Button());
    m_button_snap->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( snap_xpm ))));
    m_button_snap->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_snap  ));
    add_tooltip( m_button_snap, "Grid snap. (Fraction of Measure Length)" );
    m_entry_snap = manage( new Entry());
    m_entry_snap->set_size_request( 40, -1 );
    m_entry_snap->set_editable( false );

    m_menu_bp_measure = manage( new Menu() );
    m_menu_bw = manage( new Menu() );

    /* bw */
    m_menu_bw->items().push_back(MenuElem("1", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 1  )));
    m_menu_bw->items().push_back(MenuElem("2", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 2  )));
    m_menu_bw->items().push_back(MenuElem("4", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 4  )));
    m_menu_bw->items().push_back(MenuElem("8", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 8  )));
    m_menu_bw->items().push_back(MenuElem("16", sigc::bind(mem_fun(*this,&mainwnd::bw_button_callback), 16 )));

    char b[20];

    for( int i=0; i<16; i++ )
    {
        snprintf( b, sizeof(b), "%d", i+1 );

        /* length */
        m_menu_bp_measure->items().push_back(MenuElem(b,
                                             sigc::bind(mem_fun(*this,&mainwnd::bp_measure_button_callback),i+1 )));
    }

    /* bpm spin button */
    m_adjust_bpm = manage(new Adjustment(m_mainperf->get_bpm(), c_bpm_minimum, c_bpm_maximum, 1));
    m_spinbutton_bpm = manage( new Bpm_spinbutton( *m_adjust_bpm ));
//    m_spinbutton_bpm = manage( new SpinButton( *m_adjust_bpm ));
    m_spinbutton_bpm->set_editable( true );
    m_spinbutton_bpm->set_digits(2);                    // 2 = two decimal precision
    m_spinbutton_bpm->set_numeric();
    m_adjust_bpm->signal_value_changed().connect(
        mem_fun(*this, &mainwnd::adj_callback_bpm ));

    add_tooltip( m_spinbutton_bpm, "Adjust beats per minute (BPM) value" );
    
    Label* bpmlabel = manage(new Label("_BPM", true));
    bpmlabel->set_mnemonic_widget(*m_spinbutton_bpm);

    /* bpm tap tempo button - sequencer64 */
    m_button_tap = manage(new Button("0"));
    m_button_tap->signal_clicked().connect(mem_fun(*this, &mainwnd::tap));
    add_tooltip
    (
        m_button_tap,
        "Tap in time to set the beats per minute (BPM) value. "
        "After 5 seconds of no taps, the tap-counter will reset to 0. "
        "Also see the File / Options / Keyboard / Tap BPM key assignment."
    );
    
    /* beats per measure */
    m_button_bp_measure = manage( new Button());
    m_button_bp_measure->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( down_xpm  ))));
    m_button_bp_measure->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_bp_measure  ));
    add_tooltip( m_button_bp_measure, "Time Signature. Beats per Measure" );
    m_entry_bp_measure = manage( new Entry());
    m_entry_bp_measure->set_width_chars(2);
    m_entry_bp_measure->set_editable( false );

    /* beat width */
    m_button_bw = manage( new Button());
    m_button_bw->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( down_xpm  ))));
    m_button_bw->signal_clicked().connect(  sigc::bind<Menu *>( mem_fun( *this, &mainwnd::popup_menu), m_menu_bw  ));
    add_tooltip( m_button_bw, "Time Signature.  Length of Beat" );
    m_entry_bw = manage( new Entry());
    m_entry_bw->set_width_chars(2);
    m_entry_bw->set_editable( false );

    /* swing_amount spin buttons */
    m_adjust_swing_amount8 = manage(new Adjustment(m_mainperf->get_swing_amount8(), 0, c_max_swing_amount, 1));
    m_spinbutton_swing_amount8 = manage( new SpinButton( *m_adjust_swing_amount8 ));
    m_spinbutton_swing_amount8->set_editable( false );
    m_adjust_swing_amount8->signal_value_changed().connect(
        mem_fun(*this, &mainwnd::adj_callback_swing_amount8 ));
    add_tooltip( m_spinbutton_swing_amount8, "Adjust 1/8 swing amount" );

    m_adjust_swing_amount16 = manage(new Adjustment(m_mainperf->get_swing_amount16(), 0, c_max_swing_amount, 1));
    m_spinbutton_swing_amount16 = manage( new SpinButton( *m_adjust_swing_amount16 ));
    m_spinbutton_swing_amount16->set_editable( false );
    m_adjust_swing_amount16->signal_value_changed().connect(
        mem_fun(*this, &mainwnd::adj_callback_swing_amount16 ));
    add_tooltip( m_spinbutton_swing_amount16, "Adjust 1/16 swing amount" );

    /* undo */
    m_button_undo = manage( new Button());
    m_button_undo->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( undo_xpm  ))));
    m_button_undo->signal_clicked().connect(  mem_fun( *this, &mainwnd::undo_type));
    add_tooltip( m_button_undo, "Undo." );

    /* redo */
    m_button_redo = manage( new Button());
    m_button_redo->add( *manage( new Image(Gdk::Pixbuf::create_from_xpm_data( redo_xpm  ))));
    m_button_redo->signal_clicked().connect(  mem_fun( *this, &mainwnd::redo_type));
    add_tooltip( m_button_redo, "Redo." );

    /* expand */
    m_button_expand = manage( new Button());
    m_button_expand->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( expand_xpm ))));
    m_button_expand->signal_clicked().connect(  mem_fun( *this, &mainwnd::expand));
    add_tooltip( m_button_expand, "Expand between L and R markers." );

    /* collapse */
    m_button_collapse = manage( new Button());
    m_button_collapse->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( collapse_xpm ))));
    m_button_collapse->signal_clicked().connect(  mem_fun( *this, &mainwnd::collapse));
    add_tooltip( m_button_collapse, "Collapse between L and R markers." );

    /* copy */
    m_button_copy = manage( new Button());
    m_button_copy->add(*manage( new Image(Gdk::Pixbuf::create_from_xpm_data( copy_xpm ))));
    m_button_copy->signal_clicked().connect(  mem_fun( *this, &mainwnd::copy ));
    add_tooltip( m_button_copy, "Expand and copy between L and R markers." );

    m_hlbox->pack_end( *m_button_copy, false, false );
    m_hlbox->pack_end( *m_button_expand, false, false );
    m_hlbox->pack_end( *m_button_collapse, false, false );
    m_hlbox->pack_end( *m_button_redo, false, false );
    m_hlbox->pack_end( *m_button_undo, false, false );
    

    m_hlbox->pack_start(*bpmlabel, Gtk::PACK_SHRINK);
    m_hlbox->pack_start(*m_spinbutton_bpm, Gtk::PACK_SHRINK);

    m_hlbox->pack_start( *m_button_tap, false, false );
    
    m_hlbox->pack_start( *(manage(new VSeparator())), false, false, 4);

    m_hlbox->pack_start( *m_button_bp_measure, false, false );
    m_hlbox->pack_start( *m_entry_bp_measure, false, false );

    m_hlbox->pack_start( *(manage(new Label( "/" ))), false, false, 4);

    m_hlbox->pack_start( *m_button_bw, false, false );
    m_hlbox->pack_start( *m_entry_bw, false, false );

    m_hlbox->pack_start( *(manage(new VSeparator())), false, false, 4);

    m_hlbox->pack_start( *m_button_snap, false, false );
    m_hlbox->pack_start( *m_entry_snap, false, false );

    m_hlbox->pack_start( *m_button_xpose, false, false );
    m_hlbox->pack_start( *m_entry_xpose, false, false );

    m_hlbox->pack_start( *(manage(new Label( "swing" ))), false, false, 4);
    m_hlbox->pack_start(*m_spinbutton_swing_amount8, false, false );
    m_hlbox->pack_start(*(manage( new Label( "1/8" ))), false, false, 4);
    m_hlbox->pack_start(*m_spinbutton_swing_amount16, false, false );
    m_hlbox->pack_start(*(manage( new Label( "1/16" ))), false, false, 4);

    /* set up a vbox, put the menu in it, and add it */
    VBox *vbox = new VBox();
    vbox->pack_start(*hbox1, false, false );
    vbox->pack_start(*m_table, true, true, 0 );

    VBox *ovbox = new VBox();

    ovbox->pack_start(*m_menubar, false, false );
    ovbox->pack_start( *vbox );

    m_perfroll->set_can_focus();
    m_perfroll->grab_focus();

    /* add box */
    this->add (*ovbox);

    set_bw( 4 ); // set this first
    set_snap( 4 );
    set_bp_measure( 4 );
    set_xpose( 0 );

    /* tap button  */
    m_current_beats = 0;
    m_base_time_ms  = 0;
    m_last_time_ms  = 0;

    m_perfroll->init_before_show();

    /* show everything */
    show_all();
    p_font_renderer->init( this->get_window() );

    add_events( Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK );

    m_timeout_connect = Glib::signal_timeout().connect(
                            mem_fun(*this, &mainwnd::timer_callback), 25);

    m_sigpipe[0] = -1;
    m_sigpipe[1] = -1;
    install_signal_handlers();
}

mainwnd::~mainwnd()
{
    delete m_options;

    if (m_sigpipe[0] != -1)
        close(m_sigpipe[0]);

    if (m_sigpipe[1] != -1)
        close(m_sigpipe[1]);
}

/*
 * move through the setlist (jmp is 0 on start and 1 if right arrow, -1 for left arrow)
 */
bool mainwnd::setlist_jump(int jmp, bool a_verify)
{
    if(global_is_running)                       // don't allow jump if running
        return false;
    
    bool result = false;
    if(a_verify)                                // we will run through all the files
    {
        m_mainperf->set_setlist_index(0);       // start at zero
        jmp = 0;                                // to get the first one
    }

    while(1)
    {
        if(m_mainperf->set_setlist_index(m_mainperf->get_setlist_index() + jmp))
        {
            if(Glib::file_test(m_mainperf->get_setlist_current_file(), Glib::FILE_TEST_EXISTS))
            {
                if(open_file(m_mainperf->get_setlist_current_file()))
                {
                    if(a_verify)    // verify whole setlist
                    {
                        jmp = 1;    // after the first one set to 1 for jump
                        continue;   // keep going till the end of list
                    }
                    result = true;
                    break;
                }
                else
                {
                    Glib::ustring message = "Setlist file open error\n";
                    message += m_mainperf->get_setlist_current_file();
                    m_mainperf->error_message_gtk(message);
                    m_mainperf->set_setlist_mode(false);    // abandon ship
                    result = false;
                    break;  
                }
            }
            else
            {
                Glib::ustring message = "Setlist file does not exist\n";
                message += m_mainperf->get_setlist_current_file();
                m_mainperf->error_message_gtk(message);
                m_mainperf->set_setlist_mode(false);        // abandon ship
                result = false;
                break;  
            }
        }
        else                                                // end of file list
        {
            result = true;  // means we got to the end or beginning, without error
            break;
        }
    }
    
    if(!result)                                             // if errors occured above
        update_window_title();
    
    return result;
}

bool
mainwnd::verify_setlist_dialog()
{
    Gtk::MessageDialog warning("Do you wish the verify the setlist?\n",
                       false,
                       Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);

    auto result = warning.run();

    if (result == Gtk::RESPONSE_NO || result == Gtk::RESPONSE_DELETE_EVENT)
    {
        return false;
    }
    
    return true;
}

void
mainwnd::setlist_verify()
{
    bool result = false;
    
    result = setlist_jump(0,true);              // true is verify mode
    
    if(result)                                  // everything loaded ok
    {
        m_mainperf->set_setlist_index(0);       // set to start
        setlist_jump(0);                        // load the first file
        printf("Setlist verification was successful!\n");
    }
    else                                        // verify failed somewhere
    {
        new_file();                             // clear and start clean
    }
}

// This is the GTK timer callback, used to draw our current time and bpm
// ondd_events( the main window
bool
mainwnd::timer_callback(  )
{
    m_perfroll->redraw_dirty_tracks();
    m_perfroll->draw_progress();
    m_perfnames->redraw_dirty_tracks();

    long ticks = m_mainperf->get_tick();

    m_main_time->idle_progress( ticks );

    /* used on initial file load and during play with tempo changes from markers */
    if ( m_adjust_bpm->get_value() != m_mainperf->get_bpm())
    {
        m_adjust_bpm->set_value( m_mainperf->get_bpm());
    }

    if ( m_bp_measure != m_mainperf->get_bp_measure())
    {
        set_bp_measure( m_mainperf->get_bp_measure());
    }

    if ( m_bw != m_mainperf->get_bw())
    {
        set_bw( m_mainperf->get_bw());
    }

    if (m_button_mode->get_active() != global_song_start_mode)        // for seqroll keybinding
    {
        m_button_mode->set_active(global_song_start_mode);
    }

#ifdef JACK_SUPPORT
    if (m_button_jack->get_active() != m_mainperf->get_toggle_jack()) // for seqroll keybinding
    {
        toggle_jack();
    }

    if(global_is_running && m_button_jack->get_sensitive())
    {
        m_button_jack->set_sensitive(false);
    }
    else if(!global_is_running && !m_button_jack->get_sensitive())
    {
        m_button_jack->set_sensitive(true);
    }
#endif // JACK_SUPPORT

    if(global_is_running && m_button_mode->get_sensitive())
    {
        m_button_mode->set_sensitive(false);
    }
    else if(!global_is_running && !m_button_mode->get_sensitive())
    {
        m_button_mode->set_sensitive(true);
    }

    if (m_button_follow->get_active() != m_mainperf->get_follow_transport())
    {
        m_button_follow->set_active(m_mainperf->get_follow_transport());
    }

    if ( m_adjust_swing_amount8->get_value() != m_mainperf->get_swing_amount8())
    {
        m_adjust_swing_amount8->set_value( m_mainperf->get_swing_amount8());
    }

    if ( m_adjust_swing_amount16->get_value() != m_mainperf->get_swing_amount16())
    {
        m_adjust_swing_amount16->set_value( m_mainperf->get_swing_amount16());
    }

    if(m_mainperf->m_have_undo && !m_button_undo->get_sensitive())
    {
        m_button_undo->set_sensitive(true);
    }
    else if(!m_mainperf->m_have_undo && m_button_undo->get_sensitive())
    {
        m_button_undo->set_sensitive(false);
    }

    if(m_mainperf->m_have_redo && !m_button_redo->get_sensitive())
    {
        m_button_redo->set_sensitive(true);
    }
    else if(!m_mainperf->m_have_redo && m_button_redo->get_sensitive())
    {
        m_button_redo->set_sensitive(false);
    }

    /* Tap button - sequencer64 */
    if (m_current_beats > 0)
    {
        if (m_last_time_ms > 0)
        {
            struct timespec spec;
            clock_gettime(CLOCK_REALTIME, &spec);
            long ms = long(spec.tv_sec) * 1000;     /* seconds to ms        */
            ms += round(spec.tv_nsec * 1.0e-6);     /* nanoseconds to ms    */
            long difference = ms - m_last_time_ms;
            if (difference > 5000L)                 /* 5 second wait        */
            {
                m_current_beats = m_base_time_ms = m_last_time_ms = 0;
                set_tap_button(0);
            }
        }
    }
    
    if(m_mainperf->get_tempo_load())    /* file loading */
    {
        m_mainperf->set_tempo_load(false);
        m_tempo->load_tempo_list();
        /* reset the m_mainperf bpm for display purposes, not changing the list value*/
        m_mainperf->set_bpm(m_mainperf->get_start_tempo());
    }
    
    if(m_mainperf->get_tempo_reset())   /* play tempo markers */
    {
        m_tempo->reset_tempo_list(true); // true for updating play_list only, no need to recalc here
        m_mainperf->set_tempo_reset(false);
        /* reset the m_mainperf bpm for display purposes, not changing the list value*/
        m_mainperf->set_bpm(m_mainperf->get_start_tempo());
    }
    
    if(m_spinbutton_bpm->get_have_leave() && !m_spinbutton_bpm->get_have_typing())
    {
        if(m_spinbutton_bpm->get_hold_bpm() != m_adjust_bpm->get_value() &&
                m_spinbutton_bpm->get_hold_bpm() != 0.0)
        {
            m_tempo->push_undo(true);                   // use the hold marker
            m_spinbutton_bpm->set_have_leave(false);
        }
    }
    /* when in set list mode, tempo stop markers trigger set file increment.
     We have to let the transport completely stop before doing the 
     file loading or strange things happen*/
    if(m_mainperf->m_setlist_stop_mark && !global_is_running)
    {
        m_mainperf->m_setlist_stop_mark = false;
        setlist_jump(1);    // next file
    }
    
    return true;
}

void
mainwnd::undo_type()
{
    if(m_tempo->get_hold_undo())
        return;
    
    char type = '\0';
    if(m_mainperf->undo_vect.size() > 0)
        type = m_mainperf->undo_vect[m_mainperf->undo_vect.size() -1].type;
    else
        return;

    switch (type)
    {
    case c_undo_trigger:
        undo_trigger(m_mainperf->undo_vect[m_mainperf->undo_vect.size() -1].track);
        break;
    case c_undo_track:
        undo_track(m_mainperf->undo_vect[m_mainperf->undo_vect.size() -1].track);
        break;
    case c_undo_perf:
        undo_perf();
        break;
    case c_undo_collapse_expand:
        undo_trigger();
        break;
    case c_undo_bpm:
        undo_bpm();
        break;
    case c_undo_import:
        undo_perf(true);
        break;
    default:
        break;
    }
    m_mainperf->set_have_undo();
}

void
mainwnd::undo_trigger(int a_track)
{
    m_mainperf->pop_trigger_undo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::undo_trigger() // collapse and expand
{
    m_mainperf->pop_trigger_undo();
    m_perfroll->queue_draw();
}

void
mainwnd::undo_track( int a_track )
{
    m_mainperf->pop_track_undo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::undo_perf(bool a_import)
{
    m_mainperf->pop_perf_undo(a_import);
    m_perfroll->queue_draw();
}

void
mainwnd::undo_bpm()
{
    m_tempo->pop_undo();
}

void
mainwnd::redo_type()
{
    if(m_tempo->get_hold_undo())
        return;
    
    char type = '\0';
    if(m_mainperf->redo_vect.size() > 0)
        type = m_mainperf->redo_vect[m_mainperf->redo_vect.size() -1].type;
    else
        return;

    switch (type)
    {
    case c_undo_trigger:
        redo_trigger(m_mainperf->redo_vect[m_mainperf->redo_vect.size() - 1].track);
        break;
    case c_undo_track:
        redo_track(m_mainperf->redo_vect[m_mainperf->redo_vect.size() - 1].track);
        break;
    case c_undo_perf:
        redo_perf();
        break;
    case c_undo_collapse_expand:
        redo_trigger();
        break;
    case c_undo_bpm:
        redo_bpm();
        break;
    case c_undo_import:
        redo_perf(true);
        break;
    default:
        break;
    }
    m_mainperf->set_have_redo();
}

void
mainwnd::redo_trigger(int a_track) // single track
{
    m_mainperf->pop_trigger_redo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::redo_trigger() // collapse and expand
{
    m_mainperf->pop_trigger_redo();
    m_perfroll->queue_draw();
}

void
mainwnd::redo_track( int a_track )
{
    m_mainperf->pop_track_redo(a_track);
    m_perfroll->queue_draw();
}

void
mainwnd::redo_perf(bool a_import)
{
    m_mainperf->pop_perf_redo(a_import);
    m_perfroll->queue_draw();
}

void
mainwnd::redo_bpm()
{
    m_tempo->pop_redo();
}

void
mainwnd::start_playing()
{
    m_mainperf->start_playing();
}

void
mainwnd::stop_playing()
{
    m_mainperf->stop_playing();
}

void
mainwnd::rewind(bool a_press)
{
    if(a_press)
    {
        if(FF_RW_button_type == FF_RW_REWIND) // for key repeat, just ignore repeat
            return;
        
        FF_RW_button_type = FF_RW_REWIND;
    }
    else
        FF_RW_button_type = FF_RW_RELEASE;

    gtk_timeout_add(120,FF_RW_timeout,m_mainperf);
}

void
mainwnd::fast_forward(bool a_press)
{
    if(a_press)
    {
        if(FF_RW_button_type == FF_RW_FORWARD) // for key repeat, just ignore repeat
            return;
        
        FF_RW_button_type = FF_RW_FORWARD;
    }
    else
        FF_RW_button_type = FF_RW_RELEASE;

    gtk_timeout_add(120,FF_RW_timeout,m_mainperf);
}

void
mainwnd::collapse() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->move_triggers( false );
    m_perfroll->queue_draw();
}

void
mainwnd::copy() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->copy_triggers(  );
    m_perfroll->queue_draw();
}

void
mainwnd::expand() // all tracks
{
    m_mainperf->push_trigger_undo();
    m_mainperf->move_triggers( true );
    m_perfroll->queue_draw();
}

void
mainwnd::set_looped()
{
    m_mainperf->set_looping( m_button_loop->get_active());
}

void
mainwnd::toggle_looped() // for key mapping
{
    // Note that this will trigger the button signal callback.
    m_button_loop->set_active( ! m_button_loop->get_active() );
}

void
mainwnd::set_song_mode()
{
    global_song_start_mode = m_button_mode->get_active();
    
    bool is_active = m_button_mode->get_active();
    
    /*
     * spaces with 'Live' are to keep button width close
     * to the same when changed for cosmetic purposes.
     */
    
    std::string label = is_active ? "Song" : " Live ";
    Gtk::Label * lblptr(dynamic_cast<Gtk::Label *>
    (
         m_button_mode->get_child())
    );
    if (lblptr != NULL)
        lblptr->set_text(label);
}

void
mainwnd::toggle_song_mode()
{
    // Note that this will trigger the button signal callback.
    if(!global_is_running)
    {
        m_button_mode->set_active( ! m_button_mode->get_active() );
    }
}

void
mainwnd::set_jack_mode ()
{
    if(m_button_jack->get_active() && !global_is_running)
        m_mainperf->init_jack ();

    if(!m_button_jack->get_active() && !global_is_running)
        m_mainperf->deinit_jack ();

    if(m_mainperf->is_jack_running())
        m_button_jack->set_active(true);
    else
        m_button_jack->set_active(false);

    m_mainperf->set_jack_mode(m_mainperf->is_jack_running()); // for seqroll keybinding

    // for setting the transport tick to display in the correct location
    // FIXME currently does not work for slave from disconnected - need jack position
    if(global_song_start_mode)
    {
        m_mainperf->set_reposition(false);
        m_mainperf->set_starting_tick(m_mainperf->get_left_tick());
    }
    else
        m_mainperf->set_starting_tick(m_mainperf->get_tick());
}

void
mainwnd::toggle_jack()
{
    // Note that this will trigger the button signal callback.
    m_button_jack->set_active( ! m_button_jack->get_active() );
}

void
mainwnd::set_follow_transport()
{
    m_mainperf->set_follow_transport(m_button_follow->get_active());
}

void
mainwnd::toggle_follow_transport()
{
    // Note that this will trigger the button signal callback.
    m_button_follow->set_active( ! m_button_follow->get_active() );
}

void
mainwnd::popup_menu(Menu *a_menu)
{
    a_menu->popup(0,0);
}

void
mainwnd::set_guides()
{
    long measure_ticks = (c_ppqn * 4) * m_bp_measure / m_bw;
    long snap_ticks =  measure_ticks / m_snap;
    long beat_ticks = (c_ppqn * 4) / m_bw;
    m_perfroll->set_guides( snap_ticks, measure_ticks, beat_ticks );
    m_perftime->set_guides( snap_ticks, measure_ticks );
    m_tempo->set_guides( snap_ticks, measure_ticks );
}

void
mainwnd::set_snap( int a_snap  )
{
    char b[10];
    snprintf( b, sizeof(b), "1/%d", a_snap );
    m_entry_snap->set_text(b);

    m_snap = a_snap;
    set_guides();
}

void mainwnd::bp_measure_button_callback(int a_beats_per_measure)
{
    if(m_bp_measure != a_beats_per_measure )
    {
        set_bp_measure(a_beats_per_measure);
        global_is_modified = true;
    }
}

void mainwnd::set_bp_measure( int a_beats_per_measure )
{
    if(a_beats_per_measure < 1 || a_beats_per_measure > 16)
        a_beats_per_measure = 4;

    m_mainperf->set_bp_measure(a_beats_per_measure);

    if(a_beats_per_measure <= 7)
        set_snap(a_beats_per_measure *2);
    else
        set_snap(a_beats_per_measure);

    char b[10];
    snprintf(b, sizeof(b), "%d", a_beats_per_measure );
    m_entry_bp_measure->set_text(b);

    m_bp_measure = a_beats_per_measure;
    set_guides();
}

void mainwnd::bw_button_callback(int a_beat_width)
{
    if(m_bw != a_beat_width )
    {
        set_bw(a_beat_width);
        global_is_modified = true;
    }
}

void mainwnd::set_bw( int a_beat_width )
{
    if(a_beat_width < 1 || a_beat_width > 16)
        a_beat_width = 4;

    m_mainperf->set_bw(a_beat_width);
    char b[10];
    snprintf(b, sizeof(b), "%d", a_beat_width );
    m_entry_bw->set_text(b);

    m_bw = a_beat_width;
    set_guides();
}

void
mainwnd::set_zoom (int z)
{
    m_perfroll->set_zoom(z);
    m_perftime->set_zoom(z);
    m_tempo->set_zoom(z);
}

void
mainwnd::xpose_button_callback( int a_xpose)
{
    if(m_mainperf->get_master_midi_bus()->get_transpose() != a_xpose)
    {
        set_xpose(a_xpose);
    }
}

void
mainwnd::set_xpose( int a_xpose  )
{
    char b[11];
    snprintf( b, sizeof(b), "%+d", a_xpose );
    m_entry_xpose->set_text(b);

    m_mainperf->all_notes_off();
    m_mainperf->get_master_midi_bus()->set_transpose(a_xpose);
}

void
mainwnd::tap ()
{
    if(!m_tempo->get_hold_undo())
        m_tempo->set_hold_undo(true);
    
    double bpm = update_bpm();
    set_tap_button(m_current_beats);
    if (m_current_beats > 1)                    /* first one is useless */
        m_adjust_bpm->set_value(double(bpm));
}

void
mainwnd::set_tap_button (int beats)
{
    if(beats == 0)
    {
        m_tempo->push_undo(true);
    }
    
    Gtk::Label * tapptr(dynamic_cast<Gtk::Label *>(m_button_tap->get_child()));
    if (tapptr != nullptr)
    {
        char temp[8];
        snprintf(temp, sizeof(temp), "%d", beats);
        tapptr->set_text(temp);
    }
}

double
mainwnd::update_bpm ()
{
    double bpm = 0.0;
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    long ms = long(spec.tv_sec) * 1000;     /* seconds to milliseconds      */
    ms += round(spec.tv_nsec * 1.0e-6);     /* nanoseconds to milliseconds  */
    if (m_current_beats == 0)
    {
        m_base_time_ms = ms;
        m_last_time_ms = 0;
    }
    else if (m_current_beats >= 1)
    {
        int diffms = ms - m_base_time_ms;
        bpm = m_current_beats * 60000.0 / diffms;
        m_last_time_ms = ms;
    }
    ++m_current_beats;
    return bpm;
}

void
mainwnd::grow()
{
    m_perfroll->increment_size();
    m_perftime->increment_size();
}

void
mainwnd::delete_unused_seq()
{
    m_mainperf->delete_unused_sequences();
}

void
mainwnd::create_triggers()
{
    if(global_is_running)
        return;

    m_mainperf->create_triggers();
}

void
mainwnd::apply_song_transpose()
{
    if(global_is_running)
        return;

    if(m_mainperf->get_master_midi_bus()->get_transpose() != 0)
    {
        m_mainperf->apply_song_transpose();
        set_xpose(0);
    }
}

void
mainwnd::open_seqlist()
{
    if(m_mainperf->get_seqlist_open())
    {
        m_mainperf->set_seqlist_raise(true);
    }
    else
    {
        new seqlist(m_mainperf, this);
    }
}

void
mainwnd::set_song_mute(mute_op op)
{
    m_mainperf->set_song_mute(op);
    global_is_modified = true;
}

void
mainwnd::options_dialog()
{
    if ( m_options != NULL )
        delete m_options;
    m_options = new options( *this,  m_mainperf );
    m_options->show_all();
}

/* callback function */
void mainwnd::file_new()
{
    if (is_save())
        new_file();
}

void mainwnd::new_file()
{
    if(m_mainperf->clear_all())
    {
        m_tempo->load_tempo_list();
        set_bp_measure(4);
        set_bw(4);
        set_xpose(0);
        m_mainperf->set_start_tempo(c_bpm);
        m_mainperf->set_setlist_mode(false);
        
        global_filename = "";
        update_window_title();
        global_is_modified = false;
    }
    else
    {
        new_open_error_dialog();
        return;
    }
}

/* callback function */
void mainwnd::file_save()
{
    save_file();
}

/* callback function */
void mainwnd::file_save_as(file_type_e type, void *a_seq_or_track)
{
    Gtk::FileChooserDialog dialog("Save file as",
                                  Gtk::FILE_CHOOSER_ACTION_SAVE);
    
    switch(type)
    {
    case E_MIDI_SEQ24_FORMAT:
        dialog.set_title("Midi export (Seq 24/32/64)");
        break;
        
    case E_MIDI_SONG_FORMAT:
        dialog.set_title("Midi export song triggers");
        break;
        
    case E_MIDI_SOLO_SEQUENCE:
        dialog.set_title("Midi export sequence");
        break;
        
    case E_MIDI_SOLO_TRIGGER:
        dialog.set_title("Midi export solo trigger");
        break;
        
    case E_MIDI_SOLO_TRACK:
        dialog.set_title("Midi export solo track");
        break;
        
    default:            // Save file as -- native .s42 format
        break;
    }

    dialog.set_transient_for(*this);

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

    Gtk::FileFilter filter_midi;

    if(type == E_SEQ42_NATIVE_FILE)
    {
        filter_midi.set_name("Seq42 files");
        filter_midi.add_pattern("*.s42");
    }
    else
    {
        filter_midi.set_name("MIDI files");
        filter_midi.add_pattern("*.midi");
        filter_midi.add_pattern("*.MIDI");
        filter_midi.add_pattern("*.mid");
        filter_midi.add_pattern("*.MID");
    }

    dialog.add_filter(filter_midi);

    Gtk::FileFilter filter_any;
    filter_any.set_name("Any files");
    filter_any.add_pattern("*");
    dialog.add_filter(filter_any);

    if(type == E_SEQ42_NATIVE_FILE) // .s42
    {
        dialog.set_current_folder(last_used_dir);
    }
    else                            // midi
    {
        dialog.set_current_folder(last_midi_dir);
    }
    
    int result = dialog.run();

    switch (result)
    {
    case Gtk::RESPONSE_OK:
    {
        std::string fname = dialog.get_filename();
        Gtk::FileFilter* current_filter = dialog.get_filter();

        if ((current_filter != NULL) &&
                (current_filter->get_name() == "Seq42 files"))
        {
            // check for Seq42 file extension; if missing, add .s42
            std::string suffix = fname.substr(
                                     fname.find_last_of(".") + 1, std::string::npos);
            toLower(suffix);
            if (suffix != "s42") fname = fname + ".s42";
        }

        if ((current_filter != NULL) &&
                (current_filter->get_name() == "MIDI files"))
        {
            // check for MIDI file extension; if missing, add .midi
            std::string suffix = fname.substr(
                                     fname.find_last_of(".") + 1, std::string::npos);
            toLower(suffix);
            if ((suffix != "midi") && (suffix != "mid"))
                fname = fname + ".midi";
        }

        if (Glib::file_test(fname, Glib::FILE_TEST_EXISTS))
        {
            Gtk::MessageDialog warning(*this,
                                       "File already exists!\n"
                                       "Do you want to overwrite it?",
                                       false,
                                       Gtk::MESSAGE_WARNING, Gtk::BUTTONS_YES_NO, true);
            auto result = warning.run();

            if (result == Gtk::RESPONSE_NO)
                return;
        }

        if(type == E_SEQ42_NATIVE_FILE)
        {
            global_filename = fname;
            update_window_title();
            save_file();
        }
        else
        {
            export_midi(fname, type, a_seq_or_track); //  a_seq_or_track will be nullptr if type = E_MIDI_SEQ24_FORMAT
        }

        break;
    }

    default:
        break;
    }
}

void mainwnd::export_midi(const Glib::ustring& fn, file_type_e type, void *a_seq_or_track)
{
    bool result = false;

    midifile f(fn);

    if(type == E_MIDI_SEQ24_FORMAT || type == E_MIDI_SOLO_SEQUENCE )
        result = f.write_sequences(m_mainperf, (sequence*)a_seq_or_track);  // seq24 format will be nullptr
    else
        result = f.write_song(m_mainperf, type,(track*)a_seq_or_track);     // song format will be nullptr
    

    if (!result)
    {
        Gtk::MessageDialog errdialog
        (
            *this,
            "Error writing file.",
            false,
            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
            true
        );
        errdialog.run();
    }

    if(result)
        last_midi_dir = fn.substr(0, fn.rfind("/") + 1);
}

void mainwnd::new_open_error_dialog()
{
    Gtk::MessageDialog errdialog
    (
        *this,
        "All track edit and sequence edit\nwindows must be closed\nbefore opening a new file.",
        false,
        Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
        true
    );
    errdialog.run();
}

bool mainwnd::open_file(const Glib::ustring& fn)
{
    bool result;

    if(m_mainperf->clear_all())
    {
        set_xpose(0);

        result = m_mainperf->load(fn);

        global_is_modified = !result;

        if (!result)
        {
            Gtk::MessageDialog errdialog
            (
                *this,
                "Error reading file: " + fn,
                false,
                Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
                true
            );
            errdialog.run();
            global_filename = "";
            return false;
        }

        last_used_dir = fn.substr(0, fn.rfind("/") + 1);
        global_filename = fn;
        
        if(!m_mainperf->get_setlist_mode())            /* don't list files from setlist */
        {
            m_mainperf->add_recent_file(fn);           /* from Oli Kester's Kepler34/Sequencer 64       */
            update_recent_files_menu();
        }
        
        update_window_title();

        m_adjust_bpm->set_value( m_mainperf->get_bpm());

        m_adjust_swing_amount8->set_value( m_mainperf->get_swing_amount8());
        m_adjust_swing_amount16->set_value( m_mainperf->get_swing_amount16());
        m_tempo->load_tempo_list();
    }
    else
    {
        new_open_error_dialog();
        return false;
    }
    return true;
}

void mainwnd::export_sequence_midi(sequence *a_seq)
{
    file_save_as(E_MIDI_SOLO_SEQUENCE, a_seq);
}

void mainwnd::export_trigger_midi(track *a_track)
{
    file_save_as(E_MIDI_SOLO_TRIGGER, a_track);
}

void mainwnd::export_track_midi(int a_track)
{
    file_save_as(E_MIDI_SOLO_TRACK, m_mainperf->get_track(a_track));
}

/*callback function*/
void mainwnd::file_open()
{
    if (is_save())
        choose_file();
}

/*callback function*/
void mainwnd::file_open_setlist()
{
    if (is_save())
    {
        choose_file(true);
    }
}

void mainwnd::choose_file(const bool setlist_mode)
{
    Gtk::FileChooserDialog dialog("Open Seq42 file",
                                  Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);

    if(setlist_mode)
    	dialog.set_title("Open Setlist file");

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

    if(!setlist_mode)
    {
        Gtk::FileFilter filter_midi;
        filter_midi.set_name("Seq42 files");
        filter_midi.add_pattern("*.s42");
        dialog.add_filter(filter_midi);
    }
    
    Gtk::FileFilter filter_any;
    filter_any.set_name("Any files");
    filter_any.add_pattern("*");
    dialog.add_filter(filter_any);

    dialog.set_current_folder(last_used_dir);

    int result = dialog.run();

    switch(result)
    {
    case(Gtk::RESPONSE_OK):
        if(setlist_mode)
        {
            m_mainperf->set_setlist_mode(true);
            m_mainperf->set_setlist_file(dialog.get_filename());
            if(verify_setlist_dialog())
            {
                setlist_verify();
            }
            else
            {
                setlist_jump(0);
            }
            
            update_window_title();
        }
        else
        {
            m_mainperf->set_setlist_mode(setlist_mode); // clear setlist flag if set.
            if(!open_file(dialog.get_filename()))
                update_window_title();                  // since we cleared flag above but fail does not update
        }
    default:
        break;
    }
}

bool mainwnd::save_file()
{
    bool result = false;

    if (global_filename == "")
    {
        file_save_as(E_SEQ42_NATIVE_FILE, nullptr);
        return true;
    }

    result = m_mainperf->save(global_filename);

    if (result && !m_mainperf->get_setlist_mode())            /* don't list files from setlist */
    {
        m_mainperf->add_recent_file(global_filename);
        update_recent_files_menu();
    }
    else if (!result)
    {
        Gtk::MessageDialog errdialog
        (
            *this,
            "Error writing file.",
            false,
            Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK,
            true
        );
        errdialog.run();
    }
    global_is_modified = !result;
    return result;
}

int mainwnd::query_save_changes()
{
    Glib::ustring query_str;

    if (global_filename == "")
        query_str = "Unnamed file was changed.\nSave changes?";
    else
        query_str = "File '" + global_filename + "' was changed.\n"
                    "Save changes?";

    Gtk::MessageDialog dialog
    (
        *this,
        query_str,
        false,
        Gtk::MESSAGE_QUESTION,
        Gtk::BUTTONS_NONE,
        true
    );

    dialog.add_button(Gtk::Stock::YES, Gtk::RESPONSE_YES);
    dialog.add_button(Gtk::Stock::NO, Gtk::RESPONSE_NO);
    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

    return dialog.run();
}

bool mainwnd::is_save()
{
    bool result = false;

    if (global_is_modified)
    {
        int choice = query_save_changes();
        switch (choice)
        {
        case Gtk::RESPONSE_YES:
            if (save_file())
                result = true;
            break;
        case Gtk::RESPONSE_NO:
            result = true;
            break;
        case Gtk::RESPONSE_CANCEL:
        default:
            break;
        }
    }
    else
        result = true;

    return result;
}

/**
 *  Sets up the recent .s42 files menu.  If the menu already exists, delete it.
 *  Then recreate the new menu named "&Recent .s42 files...".  Add all of the
 *  entries present in the m_minperf->recent_files_count() list.  Hook each entry up
 *  to the open_file() function with each file-name as a parameter.  If there
 *  are none, just add a disabled "<none>" entry.
 */

#define SET_FILE    mem_fun(*this, &mainwnd::load_recent_file)

void
mainwnd::update_recent_files_menu ()
{
    if (m_menu_recent != nullptr)
    {
        /*
         * Causes a crash:
         *
         *      m_menu_file->items().remove(*m_menu_recent);    // crash!
         *      delete m_menu_recent;
         */

        m_menu_recent->items().clear();
    }
    else
    {
        m_menu_recent = manage(new Gtk::Menu());
        m_menu_file->items().push_back
        (
            MenuElem("_Recent .s42 files...", *m_menu_recent)
        );
    }

    if (m_mainperf->recent_file_count() > 0)
    {
        for (int i = 0; i < m_mainperf->recent_file_count(); ++i)
        {
            std::string filepath = m_mainperf->recent_file(i);     // shortened name
            m_menu_recent->items().push_back
            (
                MenuElem(filepath, sigc::bind(SET_FILE, i))
            );
        }
    }
    else
    {
        m_menu_recent->items().push_back
        (
            MenuElem("<none>", sigc::bind(SET_FILE, (-1)))
        );
    }
}

/**
 *  Looks up the desired recent .s42 file and opens it.  This function passes
 *  false as the shorten parameter of m_mainperf::recent_file().
 *
 * \param index
 *      Indicates which file in the list to open, ranging from 0 to the number
 *      of recent files minus 1.  If set to -1, then nothing is done.
 */

void
mainwnd::load_recent_file (int index)
{
    if (index >= 0 and index < m_mainperf->recent_file_count())
    {
        if (is_save())
        {
            std::string filepath = m_mainperf->recent_file(index, false);
            open_file(filepath);
        }
    }
}

/* convert string to lower case letters */
void
mainwnd::toLower(basic_string<char>& s)
{
    for (basic_string<char>::iterator p = s.begin();
            p != s.end(); p++)
    {
        *p = tolower(*p);
    }
}

void
mainwnd::file_import_dialog()
{
    Gtk::FileChooserDialog dialog("Import MIDI file",
                                  Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);

    Gtk::FileFilter filter_midi;
    filter_midi.set_name("MIDI files");
    filter_midi.add_pattern("*.midi");
    filter_midi.add_pattern("*.MIDI");
    filter_midi.add_pattern("*.mid");
    filter_midi.add_pattern("*.MID");
    dialog.add_filter(filter_midi);

    Gtk::FileFilter filter_any;
    filter_any.set_name("Any files");
    filter_any.add_pattern("*");
    dialog.add_filter(filter_any);

    dialog.set_current_folder(last_midi_dir);

    ButtonBox *btnbox = dialog.get_action_area();
    HBox hbox( false, 2 );

    m_adjust_load_offset = manage( new Adjustment( -1, -1, SEQ24_SCREEN_SET_SIZE - 1, 1 ));
    m_spinbutton_load_offset = manage( new SpinButton( *m_adjust_load_offset ));
    m_spinbutton_load_offset->set_editable( false );
    m_spinbutton_load_offset->set_wrap( true );
    hbox.pack_end(*m_spinbutton_load_offset, false, false );
    hbox.pack_end(*(manage( new Label("Seq24 Screen Import"))), false, false, 4);

    btnbox->pack_start(hbox, false, false );

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

    dialog.show_all_children();

    int result = dialog.run();

    //Handle the response:
    switch(result)
    {
    case(Gtk::RESPONSE_OK):
    {
        try
        {
            midifile f( dialog.get_filename() );

            if(f.parse( m_mainperf, (int) m_adjust_load_offset->get_value() ))
                last_midi_dir = dialog.get_filename().substr(0, dialog.get_filename().rfind("/") + 1);
            else return;
        }
        catch(...)
        {
            Gtk::MessageDialog errdialog
            (
                *this,
                "Error reading file.",
                false,
                Gtk::MESSAGE_ERROR,
                Gtk::BUTTONS_OK,
                true
            );
            errdialog.run();
        }
        global_is_modified = true;

        m_adjust_bpm->set_value( m_mainperf->get_bpm() );

        break;
    }

    case(Gtk::RESPONSE_CANCEL):
        break;

    default:
        break;
    }
}

/*callback function*/
void mainwnd::file_exit()
{
    if (is_save())
    {
        if (global_is_running)
            stop_playing();
        hide();
    }
}

bool
mainwnd::on_delete_event(GdkEventAny *a_e)
{
    bool result = is_save();
    if (result && global_is_running)
        stop_playing();

    return !result;
}

void
mainwnd::about_dialog()
{
    Gtk::AboutDialog dialog;
    dialog.set_transient_for(*this);
    dialog.set_name(PACKAGE_NAME);
    dialog.set_version(VERSION);
    dialog.set_comments("Interactive MIDI Sequencer\n");

    dialog.set_copyright(
        "(C) 2015 -      Stazed\n"
        "(C) 2010 - 2013 Sam Brauer\n"
        "(C) 2008 - 2009 Seq24team\n"
        "(C) 2002 - 2006 Rob C. Buse");
    dialog.set_website("https://github.com/Stazed/seq42");

    std::list<Glib::ustring> list_authors;
    list_authors.push_back("Rob C. Buse <rcb@filter24.org>");
    list_authors.push_back("Ivan Hernandez <ihernandez@kiusys.com>");
    list_authors.push_back("Guido Scholz <guido.scholz@bayernline.de>");
    list_authors.push_back("Jaakko Sipari <jaakko.sipari@gmail.com>");
    list_authors.push_back("Peter Leigh <pete.leigh@gmail.com>");
    list_authors.push_back("Anthony Green <green@redhat.com>");
    list_authors.push_back("Daniel Ellis <mail@danellis.co.uk>");
    list_authors.push_back("Kevin Meinert <kevin@subatomicglue.com>");
    list_authors.push_back("Sam Brauer <sam.brauer@gmail.com>");
    list_authors.push_back("Stazed <stazed@mapson.com>");
    dialog.set_authors(list_authors);

    std::list<Glib::ustring> list_documenters;
    list_documenters.push_back("Dana Olson <seq24@ubuntustudio.com>");
    dialog.set_documenters(list_documenters);

    dialog.show_all_children();
    dialog.run();
}

void
mainwnd::adj_callback_bpm( )
{
    if(m_mainperf->get_bpm() !=  m_adjust_bpm->get_value())
    {
        if(m_spinbutton_bpm->get_have_typing())
        {
            m_tempo->push_undo();
            m_tempo->set_start_BPM(m_adjust_bpm->get_value());
            m_spinbutton_bpm->set_hold_bpm(0.0);
            return;
        }
        if(m_spinbutton_bpm->get_have_enter())      // for user using spinner
        {
            if(!m_tempo->get_hold_undo())
                m_tempo->set_hold_undo(true);

            m_spinbutton_bpm->set_have_enter(false);
        }
        /* call to set_start_BPM will call m_mainperf->set_bpm() */
        m_tempo->set_start_BPM(m_adjust_bpm->get_value());
    }
}

void
mainwnd::adj_callback_swing_amount8( )
{
    if(m_mainperf->get_swing_amount8() != (int) m_adjust_swing_amount8->get_value())
    {
        m_mainperf->set_swing_amount8( (int) m_adjust_swing_amount8->get_value());
        global_is_modified = true;
    }
}

void
mainwnd::adj_callback_swing_amount16( )
{
    if(m_mainperf->get_swing_amount16() != (int) m_adjust_swing_amount16->get_value())
    {
        m_mainperf->set_swing_amount16( (int) m_adjust_swing_amount16->get_value());
        global_is_modified = true;
    }
}

bool
mainwnd::on_key_press_event(GdkEventKey* a_ev)
{
    // control and modifier key combinations matching
    if ( a_ev->state & GDK_CONTROL_MASK )
    {
        /* Ctrl-Z: Undo */
        if ( a_ev->keyval == GDK_z || a_ev->keyval == GDK_Z )
        {
            undo_type();
            return true;
        }
        /* Ctrl-R: Redo */
        if ( a_ev->keyval == GDK_r || a_ev->keyval == GDK_R )
        {
            redo_type();
            return true;
        }
    }

    if ( (a_ev->type == GDK_KEY_PRESS) && !(a_ev->state & GDK_MOD1_MASK) && !( a_ev->state & GDK_CONTROL_MASK ))
    {
        if ( global_print_keys )
        {
            printf( "key_press[%d]\n", a_ev->keyval );
            fflush( stdout );
        }

        if ( a_ev->keyval == m_mainperf->m_key_bpm_dn )
        {
            if(!m_tempo->get_hold_undo())
                m_tempo->set_hold_undo(true);
            
            m_tempo->set_start_BPM(m_mainperf->get_bpm() - 1);
            m_adjust_bpm->set_value(  m_mainperf->get_bpm() );
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_bpm_up )
        {
            if(!m_tempo->get_hold_undo())
                m_tempo->set_hold_undo(true);
            
            m_tempo->set_start_BPM(m_mainperf->get_bpm() + 1);
            m_adjust_bpm->set_value(  m_mainperf->get_bpm() );
            return true;
        }

        if (a_ev->keyval  == m_mainperf->m_key_tap_bpm )
        {
            tap();
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_seqlist )
        {
            open_seqlist();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_loop )
        {
            toggle_looped();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_song )
        {
            toggle_song_mode();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_follow_trans )
        {
            toggle_follow_transport();
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_forward )
        {
            fast_forward(true);
            return true;
        }

        if ( a_ev->keyval ==  m_mainperf->m_key_rewind )
        {
            rewind(true);
            return true;
        }

#ifdef JACK_SUPPORT
        if ( a_ev->keyval ==  m_mainperf->m_key_jack )
        {
            toggle_jack();
            return true;
        }
#endif // JACK_SUPPORT
        // the start/end key may be the same key (i.e. SPACE)
        // allow toggling when the same key is mapped to both triggers (i.e. SPACEBAR)
        bool dont_toggle = m_mainperf->m_key_start != m_mainperf->m_key_stop;
        if ( a_ev->keyval == m_mainperf->m_key_start && (dont_toggle || !global_is_running) )
        {
            start_playing();
            if(a_ev->keyval == GDK_space)
                return true;
        }
        else if ( a_ev->keyval == m_mainperf->m_key_stop && (dont_toggle || global_is_running) )
        {
            stop_playing();
            if(a_ev->keyval == GDK_space)
                return true;
        }
        
        if(m_mainperf->get_setlist_mode())
        {
            if ( a_ev->keyval == m_mainperf->m_key_setlist_prev )
            {
            	setlist_jump(-1);
                return true;
            }
            if ( a_ev->keyval == m_mainperf->m_key_setlist_next )
            {
            	setlist_jump(1);
                return true;
            }
        }
        
        if (a_ev->keyval == GDK_F12)
        {
            m_mainperf->print();
            fflush( stdout );
            return true;
        }
    }

    return Gtk::Window::on_key_press_event(a_ev);
}

bool
mainwnd::on_key_release_event(GdkEventKey* a_ev)
{
    if ( a_ev->type == GDK_KEY_RELEASE )
    {
        if ( a_ev->keyval ==  m_mainperf->m_key_forward )
        {
            fast_forward(false);
            return true;
        }
        if ( a_ev->keyval ==  m_mainperf->m_key_rewind )
        {
            rewind(false);
            return true;
        }
        
        if ( a_ev->keyval == m_mainperf->m_key_bpm_dn )
        {
            m_tempo->push_undo(true);
            return true;
        }
        if ( a_ev->keyval ==  m_mainperf->m_key_bpm_up )
        {
            m_tempo->push_undo(true);
            return true;
        }
    }
    return false;
}
void
mainwnd::update_window_title()
{
    std::string title;

    if(m_mainperf->get_setlist_mode())
    {
    	char num[20];
    	sprintf(num,"%02d",m_mainperf->get_setlist_index() +1);
    	title =
    		( PACKAGE )
			+ string(" - Setlist, Song ")
			+ num
			+ string(" - [")
            + Glib::filename_to_utf8(global_filename)
            + string( "]" );
    }
    else
    {
        if (global_filename == "")
            title = ( PACKAGE ) + string( " - song - unsaved" );
        else
            title =
                ( PACKAGE )
                + string( " - song - " )
                + Glib::filename_to_utf8(global_filename);
    }
    
    set_title ( title.c_str());
}

int mainwnd::m_sigpipe[2];

/* Handler for system signals (SIGUSR1, SIGINT...)
 * Write a message to the pipe and leave as soon as possible
 */
void
mainwnd::handle_signal(int sig)
{
    if (write(m_sigpipe[1], &sig, sizeof(sig)) == -1)
    {
        printf("write() failed: %s\n", std::strerror(errno));
    }
}

bool
mainwnd::install_signal_handlers()
{
    /*install pipe to forward received system signals*/
    if (pipe(m_sigpipe) < 0)
    {
        printf("pipe() failed: %s\n", std::strerror(errno));
        return false;
    }

    /*install notifier to handle pipe messages*/
    Glib::signal_io().connect(sigc::mem_fun(*this, &mainwnd::signal_action),
                              m_sigpipe[0], Glib::IO_IN);

    /*install signal handlers*/
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;

    if (sigaction(SIGUSR1, &action, NULL) == -1)
    {
        printf("sigaction() failed: %s\n", std::strerror(errno));
        return false;
    }

    if (sigaction(SIGINT, &action, NULL) == -1)
    {
        printf("sigaction() failed: %s\n", std::strerror(errno));
        return false;
    }

    return true;
}

bool
mainwnd::signal_action(Glib::IOCondition condition)
{
    int message;

    if ((condition & Glib::IO_IN) == 0)
    {
        printf("Error: unexpected IO condition\n");
        return false;
    }

    if (read(m_sigpipe[0], &message, sizeof(message)) == -1)
    {
        printf("read() failed: %s\n", std::strerror(errno));
        return false;
    }

    switch (message)
    {
    case SIGUSR1:
        save_file();
        break;
    case SIGINT:
        file_exit();
        break;
    default:
        printf("Unexpected signal received: %d\n", message);
        break;
    }
    return true;
}

int
FF_RW_timeout(void *arg)
{
    perform *p = (perform *) arg;

    if(FF_RW_button_type != FF_RW_RELEASE)
    {
        p->FF_rewind();
        if(p->m_excell_FF_RW < 60.0f)
            p->m_excell_FF_RW *= 1.1f;
        return (TRUE);
    }

    p->m_excell_FF_RW = 1.0;
    return (FALSE);
}
