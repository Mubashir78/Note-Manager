#include <ncurses.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <numeric>
#include <clocale>
#include <cwchar>

using namespace std;

#define MONOKAI_BG      COLOR_BLACK
#define MONOKAI_FG      COLOR_WHITE
#define MONOKAI_YELLOW  COLOR_YELLOW
#define MONOKAI_CYAN    COLOR_CYAN
#define MONOKAI_PURPLE  COLOR_MAGENTA

#define COLOR_NORMAL     1
#define COLOR_HIGHLIGHT  2
#define COLOR_TITLE      3
#define COLOR_EDITOR     4
#define COLOR_STATUS     5

namespace fs = filesystem;

class NoteManager {
private:
    string base_dir;
    vector<string> courses;
    vector<string> note_files;

public:
    NoteManager(const string& dir) : base_dir(dir) {
        if (!fs::exists(base_dir)) fs::create_directories(base_dir);
        load_courses();
    }

    void load_courses() {
        courses.clear();
        for (const auto& entry : fs::directory_iterator(base_dir)) {
            if (entry.is_directory()) courses.push_back(entry.path().filename().string());
        }
        sort(courses.begin(), courses.end());
    }

    vector<string> get_courses() const { return courses; }

    void create_course(const string& name) {
        fs::path path = fs::path(base_dir) / name;
        if (!fs::exists(path)) fs::create_directory(path);
        load_courses();
    }

    void delete_course(const string& name) {
        fs::remove_all(fs::path(base_dir) / name);
        load_courses();
    }

    void rename_course(const string& old_name, const string& new_name) {
        fs::rename(fs::path(base_dir) / old_name, fs::path(base_dir) / new_name);
        load_courses();
    }

    void load_notes(const string& course) {
        note_files.clear();
        fs::path path = fs::path(base_dir) / course;
        if (fs::exists(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                    note_files.push_back(entry.path().filename().string());
                }
            }
            sort(note_files.begin(), note_files.end());
        }
    }

    vector<string> get_note_names() const { return note_files; }

    string get_note_content(const string& course, const string& note) {
        ifstream file(fs::path(base_dir) / course / note);
        return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    }

    void save_note(const string& course, const string& note, const string& content) {
        ofstream file(fs::path(base_dir) / course / note);
        file << content;
    }

    void create_note(const string& course, const string& name) {
        ofstream(fs::path(base_dir) / course / (name + ".txt"));
        load_notes(course);
    }

    void delete_note(const string& course, const string& note) {
        fs::remove(fs::path(base_dir) / course / note);
        load_notes(course);
    }

    void rename_note(const string& course, const string& old_name, const string& new_name) {
        fs::rename(fs::path(base_dir) / course / old_name, 
                  fs::path(base_dir) / course / (new_name + ".txt"));
        load_notes(course);
    }
};

class MenuManager {
private:
    enum class State { MAIN, SELECT_COURSE, COURSE_MANAGEMENT, EDITING };
    
    NoteManager& notes;
    vector<State> state_stack;
    int highlight = 0;
    vector<string> current_items;
    string current_course;
    WINDOW* content_win = nullptr;
    WINDOW* edit_win = nullptr;
    string input_buffer;

    void init_colors() {
        start_color();
        use_default_colors();
        init_pair(COLOR_NORMAL, MONOKAI_FG, MONOKAI_BG);
        init_pair(COLOR_HIGHLIGHT, MONOKAI_BG, MONOKAI_CYAN);
        init_pair(COLOR_TITLE, MONOKAI_YELLOW, MONOKAI_BG);
        init_pair(COLOR_EDITOR, MONOKAI_FG, MONOKAI_BG);
        init_pair(COLOR_STATUS, MONOKAI_PURPLE, MONOKAI_BG);
    }

    WINDOW* create_window(int h, int w, int y, int x) {
        WINDOW* win = newwin(h, w, y, x);
        wbkgd(win, COLOR_PAIR(COLOR_NORMAL));
        return win;
    }

    void show_message(const string& msg) {
        WINDOW* msg_win = create_window(3, msg.length() + 4, LINES/2 - 1, (COLS - msg.length() - 4)/2);
        wattron(msg_win, COLOR_PAIR(COLOR_TITLE));
        box(msg_win, 0, 0);
        mvwprintw(msg_win, 1, 2, "%s", msg.c_str());
        wattroff(msg_win, COLOR_PAIR(COLOR_TITLE));
        wrefresh(msg_win);
        nodelay(stdscr, FALSE);
        getch();
        nodelay(stdscr, TRUE);
        delwin(msg_win);
    }

    string get_input(const string& prompt) {
        echo();
        curs_set(1);
        nodelay(stdscr, FALSE);
        char buffer[256];
        mvprintw(LINES - 2, 2, "%s", prompt.c_str());
        clrtoeol();
        getstr(buffer);
        noecho();
        curs_set(0);
        nodelay(stdscr, TRUE);
        move(LINES - 2, 2);
        clrtoeol();
        refresh();
        return string(buffer);
    }

    void draw_main() {
        if (!content_win) content_win = create_window(LINES-4, COLS-4, 2, 2);
        werase(content_win);
        
        wattron(content_win, COLOR_PAIR(COLOR_TITLE));
        box(content_win, 0, 0);
        mvwprintw(content_win, 1, 2, "Note Manager");
        wattroff(content_win, COLOR_PAIR(COLOR_TITLE));
        
        const vector<string> options = {"Manage Courses & Notes", "Exit"};
        for(size_t i=0; i<options.size(); ++i) {
            if(i == highlight) wattron(content_win, COLOR_PAIR(COLOR_HIGHLIGHT));
            mvwprintw(content_win, i+3, 2, "%s", options[i].c_str());
            wattroff(content_win, COLOR_PAIR(COLOR_HIGHLIGHT));
        }
        
        wattron(content_win, COLOR_PAIR(COLOR_STATUS));
        mvwprintw(content_win, LINES-6, 2, "Arrows: Navigate | Enter: Select | Esc: Quit");
        wattroff(content_win, COLOR_PAIR(COLOR_STATUS));
        
        wrefresh(content_win);
    }

    void draw_list(const string& title, bool show_controls) {
        if (!content_win) content_win = create_window(LINES-4, COLS-4, 2, 2);
        werase(content_win);
        
        wattron(content_win, COLOR_PAIR(COLOR_TITLE));
        box(content_win, 0, 0);
        mvwprintw(content_win, 1, 2, "%s", title.c_str());
        wattroff(content_win, COLOR_PAIR(COLOR_TITLE));
        
        if(current_items.empty()) {
            wattron(content_win, COLOR_PAIR(COLOR_STATUS));
            if (show_controls) {
                mvwprintw(content_win, 3, 2, "No items found. Press N to create new.");
            } else {
                mvwprintw(content_win, 3, 2, "No items found.");
            }
            wattroff(content_win, COLOR_PAIR(COLOR_STATUS));
        }
        else {
            for(size_t i=0; i<current_items.size(); ++i) {
                if(i == highlight) wattron(content_win, COLOR_PAIR(COLOR_HIGHLIGHT));
                mvwprintw(content_win, i+3, 2, "%s", current_items[i].c_str());
                wattroff(content_win, COLOR_PAIR(COLOR_HIGHLIGHT));
            }
        }
        
        wattron(content_win, COLOR_PAIR(COLOR_STATUS));
        if (show_controls) {
            mvwprintw(content_win, LINES-6, 2, 
                     "R: Rename | D: Delete | N: New | Enter: Edit | Esc: Back");
        } else {
            mvwprintw(content_win, LINES-6, 2, 
                     "Enter: Select | Esc: Back");
        }
        wattroff(content_win, COLOR_PAIR(COLOR_STATUS));
        
        wrefresh(content_win);
    }

    void edit_note(const string& note) {
        string content = notes.get_note_content(current_course, note);
        vector<string> lines;
        size_t pos;
        while((pos = content.find('\n')) != string::npos) {
            lines.push_back(content.substr(0, pos));
            content.erase(0, pos+1);
        }
        lines.push_back(content);

        edit_win = create_window(LINES-4, COLS-4, 2, 2);
        keypad(edit_win, TRUE);
        curs_set(1);
        echo();
        
        int ch;
        size_t cline = 0, cpos = 0;
        bool editing = true;

        while(editing) {
            werase(edit_win);
            wattron(edit_win, COLOR_PAIR(COLOR_TITLE));
            box(edit_win, 0, 0);
            mvwprintw(edit_win, 0, 2, " Editing: %s ", note.c_str());
            wattroff(edit_win, COLOR_PAIR(COLOR_TITLE));
            
            wattron(edit_win, COLOR_PAIR(COLOR_EDITOR));
            for(size_t i=0; i<lines.size(); ++i)
                mvwprintw(edit_win, i+1, 1, "%s", lines[i].c_str());
            
            wattron(edit_win, COLOR_PAIR(COLOR_STATUS));
            mvwprintw(edit_win, LINES-5, 1, " ESC: Save & Exit | Ctrl+S: Save");
            wattroff(edit_win, COLOR_PAIR(COLOR_STATUS));
            
            wmove(edit_win, cline+1, cpos+1);
            wrefresh(edit_win);

            ch = wgetch(edit_win);
            switch(ch) {
                case KEY_UP: 
                    if(cline > 0) {
                        cline--;
                        cpos = min(cpos, lines[cline].size());
                    }
                    break;
                case KEY_DOWN: 
                    if(cline < lines.size()-1) {
                        cline++;
                        cpos = min(cpos, lines[cline].size());
                    }
                    break;
                case KEY_LEFT: 
                    if(cpos > 0) cpos--; 
                    break;
                case KEY_RIGHT: 
                    if(cpos < lines[cline].size()) cpos++; 
                    break;
                case 10: 
                    lines.insert(lines.begin()+cline+1, lines[cline].substr(cpos));
                    lines[cline] = lines[cline].substr(0, cpos);
                    cline++; 
                    cpos = 0;
                    break;
                case KEY_BACKSPACE:
                case 127:
                    if(cpos > 0) {
                        lines[cline].erase(--cpos, 1);
                    } else if(cline > 0) {
                        cpos = lines[cline-1].size();
                        lines[cline-1] += lines[cline];
                        lines.erase(lines.begin() + cline);
                        cline--;
                    }
                    break;
                case 27: editing = false; break;
                case 19: 
                    notes.save_note(current_course, note, 
                        accumulate(lines.begin(), lines.end(), string(), 
                            [](string a, string b) { return a + b + "\n"; }));
                    show_message("Note saved!");
                    break;
                case KEY_RESIZE:
                    // Handle resize during editing (not ideal but basic)
                    delwin(edit_win);
                    edit_win = nullptr;
                    editing = false;
                    break;
                default:
                    if(isprint(ch)) {
                        lines[cline].insert(cpos, 1, ch);
                        cpos++;
                    }
            }
        }

        notes.save_note(current_course, note, 
            accumulate(lines.begin(), lines.end(), string(), 
                [](string a, string b) { return a + b + "\n"; }));
                
        keypad(edit_win, FALSE);
        curs_set(0);
        noecho();
        delwin(edit_win);
        edit_win = nullptr;
        if(content_win) {
            touchwin(content_win);
            wrefresh(content_win);
        }
        touchwin(stdscr);
        refresh();
    }

public:
    MenuManager(NoteManager& nm) : notes(nm) {
        setlocale(LC_ALL, "");
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
        init_colors();
        bkgd(COLOR_PAIR(COLOR_NORMAL));
        state_stack.push_back(State::MAIN);
    }

    ~MenuManager() {
        delwin(content_win);
        if(edit_win) delwin(edit_win);
        endwin();
    }

    void run() {
        nodelay(stdscr, TRUE);
        int ch;
        while(!state_stack.empty()) {
            State current_state = state_stack.back();
            switch(current_state) {
                case State::MAIN: {
                    draw_main();
                    ch = getch();
                    if (ch == KEY_UP) highlight = highlight ? highlight-1 : 1;
                    if (ch == KEY_DOWN) highlight = (highlight == 1) ? 0 : highlight+1;
                    if (ch == 10) {
                        if (highlight == 0) {
                            current_items = notes.get_courses();
                            state_stack.push_back(State::SELECT_COURSE);
                            highlight = 0;
                        } else {
                            state_stack.pop_back();
                        }
                    }
                    else if (ch == 27) state_stack.pop_back();
                    else if (ch == KEY_RESIZE) {
                        delwin(content_win);
                        content_win = nullptr;
                    }
                    break;
                }

                case State::SELECT_COURSE: {
                    current_items = notes.get_courses(); // Reload courses each time
                    draw_list("Select Course", false);
                    ch = getch();
                    if (ch == KEY_UP) highlight = highlight ? highlight-1 : current_items.size()-1;
                    if (ch == KEY_DOWN) highlight = (highlight == current_items.size()-1) ? 0 : highlight+1;
                    if (ch == 10) {
                        if (!current_items.empty()) {
                            current_course = current_items[highlight];
                            notes.load_notes(current_course);
                            current_items = notes.get_note_names();
                            state_stack.push_back(State::COURSE_MANAGEMENT);
                            highlight = 0;
                        }
                    } else if (ch == 27) {
                        state_stack.pop_back();
                    } else if (ch == KEY_RESIZE) {
                        delwin(content_win);
                        content_win = nullptr;
                    }
                    break;
                }

                case State::COURSE_MANAGEMENT: {
                    draw_list("Managing: " + current_course, true);
                    ch = getch();
                    if (ch == KEY_UP) highlight = highlight ? highlight-1 : current_items.size()-1;
                    if (ch == KEY_DOWN) highlight = (highlight == current_items.size()-1) ? 0 : highlight+1;
                    if (ch == 'n' || ch == 'N') {
                        string name = get_input("New note name (without .txt): ");
                        if (!name.empty()) {
                            notes.create_note(current_course, name);
                            current_items = notes.get_note_names();
                        }
                    }
                    else if (ch == 'r' || ch == 'R') {
                        if (!current_items.empty()) {
                            string new_name = get_input("New course name: ");
                            if (!new_name.empty()) {
                                notes.rename_course(current_course, new_name);
                                current_course = new_name;
                                current_items = notes.get_note_names();
                            }
                        }
                    }
                    else if (ch == 'd' || ch == 'D') {
                        if (!current_items.empty()) {
                            notes.delete_course(current_course);
                            state_stack.pop_back();
                            state_stack.pop_back();
                            current_items = notes.get_courses();
                            highlight = 0;
                        }
                    }
                    else if (ch == 10) {
                        if (!current_items.empty()) {
                            edit_note(current_items[highlight]);
                        }
                    }
                    else if (ch == 27) {
                        state_stack.pop_back();
                    }
                    else if (ch == KEY_RESIZE) {
                        delwin(content_win);
                        content_win = nullptr;
                    }
                    break;
                }

                default: break;
            }
            napms(50);
        }
    }
};

int main() {
    setlocale(LC_ALL, "");
    string path = string(getenv("HOME")) + "/Documents/Notes";
    NoteManager notes(path);
    MenuManager menu(notes);
    menu.run();
    return 0;
}
