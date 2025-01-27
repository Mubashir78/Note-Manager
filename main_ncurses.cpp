#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <functional>
#include <filesystem>
#include <string>
#include <ncurses.h>
#include <vector>
using namespace std;


class Note {
public:
    string title, file_path;
    static int next_ID; int ID;

    Note(string title, string folder_path) : title(title){
        file_path += folder_path + "/" + title + ".txt";
        ID = next_ID++;
    }

    Note(string title, string folder_path, string desc) : title(title){
        file_path += folder_path + "/" + title + ".txt";
        ID = next_ID++; write(desc);
    }

    void write(string desc){
        ofstream note_file(file_path);

        if (!note_file.is_open()){
            cerr << "Failed to create file \"" << file_path << "\"\n";
            return;
        }

        note_file << desc << endl;
        note_file.close();
    }
};

int next_ID = 1;

class Course_Notes {
public:
    vector<Note*> notelist;
    string course, course_note_dir;

    Course_Notes(string default_dir, string course) : course(course){
        course_note_dir = default_dir + "/" + course;
    }

    void load_notes(){
        if (!(filesystem::exists(course_note_dir) || filesystem::is_directory(course_note_dir))){
            cerr << "\nERROR: Directory \"" << course_note_dir << "\" doesn't exist yet!\n";
            return;
        }

        for (const auto& file : filesystem::directory_iterator(course_note_dir)){
            if (file.is_regular_file()){
                notelist.push_back(new Note(file.path().filename().string(), course_note_dir));
            }
        }
    }

    void show_notes(){
        cout << endl << course << " Notes:\n";

        for (const auto& note : notelist) cout << note->ID << "- " << note->title << endl;
        cout << endl;
    }

    void write_note(string title, string desc){
        notelist.push_back(new Note(title, course_note_dir, desc));
    }

    void delete_note(int ID){
        auto note_ptr = find_if(notelist.begin(), notelist.end(), [ID](Note* ptr) {
            return ptr->ID == ID;
        });

        if (note_ptr >= notelist.end()){
            cerr << "Note with ID " << ID << " doesn't exist!\n";
            return;
        }

        delete *note_ptr; notelist.erase(note_ptr);
    }
};

class Note_Manager {
    vector<Course_Notes*> course_notes;
    string note_dir = string(getenv("HOME")) + "/Documents/Notes";

public:
    Note_Manager(){
        if (!(filesystem::exists(note_dir) || filesystem::is_directory(note_dir))){
            char usr_input;
            cout << "ERROR: \"" << note_dir << "\" not created.\nDo you wish to create it now? (y/n): ";
            cin >> usr_input;

            switch (usr_input){
                case 'y':
                case 'Y': {
                    filesystem::create_directories(note_dir);
                    cout << "\nDirectory created.\n";
                } break;

                default: {
                    cout << "\nNot creating directory.\n";
                    return;
                }
            }
        }

        for (const auto& folder : filesystem::directory_iterator(note_dir)){
            if (folder.is_directory()){
                string course = folder.path().filename().string();
                course_notes.push_back(new Course_Notes(note_dir, course));
            }
        }
    }

    void add_course(string course){
        course_notes.push_back(new Course_Notes(note_dir, course));
    }

    void list_course(){
        if (!course_notes.size())
            cout << "\nERROR: No courses found!\n";

        else {
            cout << "\nFound current courses:\n";

            for (int i=0; i < course_notes.size(); i++){}
               // print_centered_left_aligned(to_string(i+1) + "- " + course_notes[i]->course);
        }

        cout << "\n\nPress any key to continue...";
        ios_base::sync_with_stdio(false);

        if (cin.peek() == '\n') cin.get();

        ios_base::sync_with_stdio(true);
    }

    void delete_course(string course){
        string course_note_dir = note_dir + "/" + course;

        for (const auto& course_folder : course_notes){
            if (course_folder->course == course && filesystem::exists(course_note_dir)
                && filesystem::is_directory(course_note_dir)){
                filesystem::remove_all(course_note_dir);

                cout << "\nDirectory \"" << course_note_dir << "\" deleted successfully.\n";
                break;
            }
        }
    }
};

class Menu_Item {
public:
    string text;
    function<void()> method;

    Menu_Item(const string& text, const function<void()> method) : text(text), method(method){}
};

class Menu {
public:
    vector<Menu_Item> items;
    string title, prompt = "Use arrow keys to navigate, Enter to select.";

    Menu(const string& title) : title(title){}
    Menu(const string& title, const string& prompt) : title(title), prompt(prompt){}

    void add_item(const string& text, function<void()> method){ items.emplace_back(text, method); }
    void draw(int selected){
        clear();

        mvprintw(0, (COLS - title.length())/2, "%s", title.c_str());

        int start_Y = (LINES - items.size() * 2) / 2, max_len = 0;

        for (const auto& item : items)
            max_len = max(max_len, (int) item.text.length() + 3);

        int start_X = (COLS - max_len) / 2;

        for (int i=0; i<items.size(); i++){
            if (i == selected) attron(A_REVERSE);

            mvprintw(start_Y + i*2, start_X, "%d- %s", i+1, items[i].text.c_str());

            if (i == selected) attroff(A_REVERSE);
        }

        mvprintw(LINES-1, (COLS - prompt.length())/2, "%s", prompt.c_str());
        refresh();
    }

    void execute(int selected){ items[selected].method(); }
};

class Menu_Manager {
public:
    Menu menu; int selected; bool is_running = true;
    Note_Manager& manager;

    Menu_Manager(const Menu& menu, Note_Manager& manager) : menu(menu), manager(manager), selected(0){}

    void run(){
        init_ncurses();
        program_loop();
        close_ncurses();
    }

    void init_ncurses(){
        initscr(); noecho(); cbreak(); keypad(stdscr, TRUE); curs_set(0);
    }

    void program_loop(){
        while (is_running){
            draw_menu();
            refresh();
            handle_input();
        }
    }

    void draw_menu(){ menu.draw(selected); }

    void handle_input(){
        int ch = getch();

        switch(ch){
            case KEY_UP: {
                selected += menu.items.size() - 1;
                selected %= menu.items.size();
            } break;

            case KEY_DOWN: {
                selected += 1;
                selected %= menu.items.size();
            } break;

            case '\n': {
                menu.items[selected].method();
            } break;

            case 'q': return;
        }
    }

    void move_up(){
        selected += menu.items.size() - 1;
        selected %= menu.items.size();
    }

    void move_down(){
        selected += 1;
        selected %= menu.items.size();
    }

    void close_ncurses(){ endwin(); }
};


int main(int argc, char** argv){
    Note_Manager note_manager;
    Menu()
    return 0;
}
