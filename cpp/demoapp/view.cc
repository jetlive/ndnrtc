//
//  view.c
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <iostream>
#include <cstdlib>
#include <curses.h>
#include <menu.h>
#include "view.h"
#include <pthread.h>
#include <panel.h>

#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "ndnrtc-library.h"

const char *menu[] = {
    "1 start media publishing",
    "2 stop media publishing",
    "3 fetch stream",
    "4 stop fetching stream",
    "5 load config file",
    "6 loopback mode",
    "7 show statistics",
    "8 arrange all windows",
    "0 exit",
};

#define N_CHOICES 9

using namespace std;

ITEM **my_items;
MENU *my_menu;
ITEM *cur_item;
WINDOW *main_menu_win,
*log_box_win, *log_win,
*stat_box_win;

static pthread_mutex_t OutputMutex((pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER);
bool stat_on;

#ifdef SHOW_STATISTICS
WINDOW *stat_win;
PANEL *stat_panel;
#endif

void refreshWindows();

void initView()
{
    initscr();
	raw();
	keypad(stdscr, TRUE);
    
    int w,h;
    getmaxyx(stdscr, h, w);
    
    int mid = w;
    int logHeight = h/2;
    
    refresh();
    
    { // main window
        main_menu_win = newwin(h-logHeight, mid, 0, 0);
        keypad(main_menu_win, TRUE);
    }
    { // log window
        pthread_mutex_init(&OutputMutex, NULL);
        
        int statusWinW = w, statusWinH = h-logHeight-3;
        int statusWinY = h-logHeight, statusWinX = 0;
        
        log_box_win = newwin(statusWinH, statusWinW, statusWinY, statusWinX);
        log_win = derwin(log_box_win, statusWinH-2, statusWinW-2, 1, 1);
        scrollok(log_win, TRUE);
    }
#ifdef SHOW_STATISTICS
    {
        stat_on = false;
        int statWinW = w, statWinH = h-2;
        int statWinX = 0, statWinY = 0;
        
        stat_box_win = newwin(statWinH, statWinW, statWinY, statWinX);
        stat_win = derwin(stat_box_win, statWinH-2, statWinW-2, 1, 1);
        
        stat_panel = new_panel(stat_win);
    }
#endif
    refreshWindows();
}

void refreshWindows()
{
    pthread_mutex_lock(&OutputMutex);
    
    if (stat_on)
    {
        box(stat_box_win, 0, 0);
        wrefresh(stat_box_win);
        update_panels();
        doupdate();
    }
    else
    {
        box(main_menu_win, 0, 0);
        wrefresh(main_menu_win);
        
        box(log_box_win, 0, 0);
        wrefresh(log_box_win);
    }
    
    pthread_mutex_unlock(&OutputMutex);
}

void freeView()
{
	endwin();
}

int plotMainMenu()
{
    int c;
	int n_choices, i;
    
    n_choices = N_CHOICES;
	my_items = (ITEM **)calloc(n_choices + 1, sizeof(ITEM *));
    
	for(i = 0; i < n_choices; ++i)
        my_items[i] = new_item(menu[i], NULL);
	my_items[n_choices] = (ITEM *)NULL;
    
    clear();
    mvwprintw(main_menu_win, 1, 1, "%s", "Main menu:");
    
	my_menu = new_menu((ITEM **)my_items);
    
    set_menu_win(my_menu, main_menu_win);
    
    int winW, winH;
    
    getmaxyx(main_menu_win, winH, winW);
    set_menu_sub(my_menu, derwin(main_menu_win, winH-2, winW-2, 2, 1));
    set_menu_mark(my_menu, ">");
    
	mvprintw(LINES - 2, 0, "Esc or '0' to Exit");
    refresh();
    
	post_menu(my_menu);
	wrefresh(main_menu_win);
    refreshWindows();
    
	while((c = getch()) != 27 && c!= 3 && c != KEY_ENTER && c != '\n' && !(c >= '0' && c <= (n_choices-1+'0')))
	{
        switch(c)
        {	case KEY_DOWN:
                menu_driver(my_menu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
				menu_driver(my_menu, REQ_UP_ITEM);
				break;
		}
        refreshWindows();
	}
    
    unpost_menu(my_menu);
    int res = 0;
    if (c != 27 && c != '0' && c != 3)
    {
        int menuidx = item_index(current_item(my_menu));
        
        if (c != KEY_ENTER && c != '\n')
            res = c-'0';
        else
            res = (menuidx == n_choices-1)? 0 : menuidx+1;
    }
    
    free_menu(my_menu);
    for(i = 0; i < n_choices; ++i)
        free_item(my_items[i]);
    echo();
    
    refresh();
    
    return res;
}

int handleStatPanel()
{
    int c;
    
	mvprintw(LINES - 2, 0, "'0' to dismiss statistics, arrows - switch between multiple producers");
    
	while((c = getch()) != '0')
	{
        switch(c)
        {
            case KEY_LEFT:
                return 9;
				break;
			case KEY_RIGHT:
                return 10;
				break;
		}
        refreshWindows();
	}
    return 7;
}

int plotmenu()
{
    noecho();
    
    if (stat_on)
        return handleStatPanel();
    else
        return plotMainMenu();
}

string getInput(const string &hintText)
{
    char str[256];
    
    mvwprintw(main_menu_win, 1, 1, "%s", hintText.c_str());
    refreshWindows();
    
    wgetstr(main_menu_win, str);
    wclear(main_menu_win);
    
    return string(str);
}

string getInput(const char *hintText, ...)
{
    va_list args;
    static char tempBuf[256];
    
    va_start(args, hintText);
    memset(tempBuf, 0, 256);
    vsprintf(tempBuf, hintText, args);
    va_end(args);
    
    std::string hint(tempBuf);
    
    return getInput(hint);
}

void statusUpdate(const std::string &time, const std::string &text)
{
    if (!stat_on)
    {
        pthread_mutex_lock(&OutputMutex);
        wattron(log_win, A_BOLD);
        wprintw(log_win, time.c_str());
        wattroff(log_win, A_BOLD);
        
        wprintw(log_win, "%s\n", text.c_str());
        wrefresh(log_win);
        pthread_mutex_unlock(&OutputMutex);
        
        refreshWindows();
    }
}

#ifdef SHOW_STATISTICS
void toggleStat()
{
    stat_on = !stat_on;
    if (stat_on)
        show_panel(stat_panel);
    else
        hide_panel(stat_panel);
    
    refreshWindows();
}

void printStat(ndnrtc::SenderChannelPerformance sendStat,
               ndnrtc::ReceiverChannelPerformance stat,
               int x, int y)
{
    mvwprintw(stat_win, y++, x, "");
    mvwprintw(stat_win, y++, x, "%10d", sendStat.lastFrameNo_);

    mvwprintw(stat_win, y++, x, "%10.2f", sendStat.nFramesPerSec_);
    mvwprintw(stat_win, y++, x, "%10.2f", sendStat.encodingRate_);
    mvwprintw(stat_win, y++, x, "%10d", sendStat.nDroppedByEncoder_);
    mvwprintw(stat_win, y++, x, "%10.2f", sendStat.nBytesPerSec_*8./1000.);
    mvwprintw(stat_win, y++, x, "%10.2f", sendStat.nBytesPerSec_*8./1000./sendStat.encodingRate_*25.);
    
    mvwprintw(stat_win, y++, x, "");
    mvwprintw(stat_win, y++, x, "");
    
    mvwprintw(stat_win, y++, x, "%10.2f", stat.actualProducerRate_);
    mvwprintw(stat_win, y++, x, "%10.2f", stat.segmentsFrequency_);
    mvwprintw(stat_win, y++, x, "%10.2f", stat.interestFrequency_);
    mvwprintw(stat_win, y++, x, "%10d", stat.jitterTargetMs_);
    mvwprintw(stat_win, y++, x, "%10d", stat.jitterEstimationMs_);
    mvwprintw(stat_win, y++, x, "%10d", stat.jitterPlayableMs_);
    mvwprintw(stat_win, y++, x, "%10.3f", stat.latency_);
    
    mvwprintw(stat_win, y++, x, "%10.2f", stat.nBytesPerSec_*8./1000.);
    
    double loss = (stat.nReceived_ == 0)?0.:
    ((double)stat.nLost_/(double)stat.nReceived_);

    mvwprintw(stat_win, y++, x, "%10d", stat.rebufferingEvents_);
    mvwprintw(stat_win, y++, x, "%10d", stat.nReceived_);
    mvwprintw(stat_win, y++, x, "%10d", stat.nPlayed_);
    mvwprintw(stat_win, y++, x, "%5d %4.1f%%", stat.nMissed_,
              ((double)stat.nMissed_/(double)stat.nPlayed_)*100);
    mvwprintw(stat_win, y++, x, "%10d", stat.nIncomplete_);
//    mvwprintw(stat_win, y++, x, "%5d %4.1f%%",
//              stat.nLost_,
//              ((double)stat.nLost_/(double)stat.nReceived_)*100);
    mvwprintw(stat_win, y++, x, "%10d", stat.nRescued_);
    mvwprintw(stat_win, y++, x, "%10d", stat.nRecovered_);
    mvwprintw(stat_win, y++, x, "%10d", stat.rtxNum_);
    mvwprintw(stat_win, y++, x, "%10.2f", stat.rtxFreq_);
    
    mvwprintw(stat_win, y++, x, "%10.2f", stat.segNumDelta_);
    mvwprintw(stat_win, y++, x, "%10.2f", stat.segNumKey_);
    
    mvwprintw(stat_win, y++, x, "%10.3f", stat.rttEstimation_);
}

void updateStat(ndnrtc::NdnLibStatistics &stat,
                const std::string &pubPrefix,
                const std::string &fetchPrefix)
{
    int w,h, x = 0, y = 0;
    
    wclear(stat_box_win);
    
    getmaxyx(stat_win, h, w);
    
    pthread_mutex_lock(&OutputMutex);
    mvwprintw(stat_win, y++, x, "> publishing: %s", pubPrefix.c_str());
    
    mvwprintw(stat_win, y++, x, "sent frames num: ");
    mvwprintw(stat_win, y++, x, "capturing (FPS): ");
    mvwprintw(stat_win, y++, x, "encoding (FPS): ");
    mvwprintw(stat_win, y++, x, "encoder dropped: ");
    mvwprintw(stat_win, y++, x, "OUT rate (kbit/s): ");
    mvwprintw(stat_win, y++, x, "OUT rate (25fps): ");
    
    mvwprintw(stat_win, y++, x, "");
    mvwprintw(stat_win, y++, x, "> fetching: %s", fetchPrefix.c_str());
    
    mvwprintw(stat_win, y++, x, "producer rate: ");
    mvwprintw(stat_win, y++, x, "IN data (seg/sec): ");
    mvwprintw(stat_win, y++, x, "OUT intrst (/sec): ");

    mvwprintw(stat_win, y++, x, "j target (ms): ");
    mvwprintw(stat_win, y++, x, "j estimate (ms): ");
    mvwprintw(stat_win, y++, x, "j playable (ms): ");
    mvwprintw(stat_win, y++, x, "plbck latency: ");
    
    mvwprintw(stat_win, y++, x, "IN rate (kbit/s): ");
    
    mvwprintw(stat_win, y++, x, "# rebufferings: ");
    mvwprintw(stat_win, y++, x, "# received frames: ");
    mvwprintw(stat_win, y++, x, "# played frames: ");
    mvwprintw(stat_win, y++, x, "# missed frames: ");
    mvwprintw(stat_win, y++, x, "# incomplete : ");
//    mvwprintw(stat_win, y++, x, "# late frames: ");
    
    mvwprintw(stat_win, y++, x, "# rescued frames: ");
    mvwprintw(stat_win, y++, x, "# recovered frames: ");
    mvwprintw(stat_win, y++, x, "rtx #: ");
    mvwprintw(stat_win, y++, x, "rtx freq: ");

    mvwprintw(stat_win, y++, x, "avg seg delta: ");
    mvwprintw(stat_win, y++, x, "avg seg key: ");
    
    mvwprintw(stat_win, y++, x, "RTT estimation: ");
    
    // print video
    printStat(stat.sendStat_.videoStat_, stat.receiveStat_.videoStat_, w/4, 0);
    
    // print audio
    printStat(stat.sendStat_.audioStat_, stat.receiveStat_.audioStat_, w/2, 0);
   
    pthread_mutex_unlock(&OutputMutex);
    
    refreshWindows();
}

#endif