#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#include <iomanip>
#include <memory>
using namespace std;

int get_console_width(){
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

void print_centered(const string& text){
    int spaces = max(0, (get_console_width() - (int)text.length()) / 2);
    cout << setw(spaces) << "" << text << endl;
}

void print_centered_left_aligned(const string& text){
    int spaces = max(0, (int)(get_console_width() / 2.5));
    cout << setw(spaces) << "" << text << endl;
}

int next_ID = 1;

class Note {
public:
    string title, file_path;
    int ID;

    Note(string title, string folder_path) : title(title){
        file_path = folder_path + "/" + title + ".txt";
        ID = next_ID++;
    }

    Note(string title, string folder_path, string desc) : title(title){
        file_path = folder_path + "/" + title + ".txt";
        ID = next_ID++;
        write(desc);
    }

    void write(const string& desc){
        ofstream note_file(file_path);
        if (!note_file.is_open()){
            cerr << "Failed to create file \"" << file_path << "\"\n";
            return;
        }
        note_file << desc << endl;
    }
};

class Course_Notes {
public:
    vector<unique_ptr<Note>> notelist;
    string course, course_note_dir;

    Course_Notes(const string& default_dir, const string& course) : course(course){
        course_note_dir = default_dir + "/" + course;
    }

    void load_notes(){
        if (!filesystem::exists(course_note_dir) || !filesystem::is_directory(course_note_dir)){
            cerr << "\nERROR: Directory \"" << course_note_dir << "\" doesn't exist yet!\n";
            return;
        }

        for (const auto& file : filesystem::directory_iterator(course_note_dir)){
            if (file.is_regular_file()){
                notelist.push_back(make_unique<Note>(file.path().filename().string(), course_note_dir));
            }
        }
    }

    void show_notes(){
        cout << endl << course << " Notes:\n";
        for (const auto& note : notelist)
            cout << note->ID << "- " << note->title << endl;
        cout << endl;
    }

    void write_note(const string& title, const string& desc){
        notelist.push_back(make_unique<Note>(title, course_note_dir, desc));
    }

    void delete_note(int ID){
        auto it = remove_if(notelist.begin(), notelist.end(), [ID](const unique_ptr<Note>& ptr) {
            return ptr->ID == ID;
        });
        if (it == notelist.end()){
            cerr << "Note with ID " << ID << " doesn't exist!\n";
            return;
        }
        notelist.erase(it, notelist.end());
    }
};

class Note_Manager {
    vector<unique_ptr<Course_Notes>> course_notes;
    string note_dir = string(getenv("HOME")) + "/Documents/Notes";

public:
    Note_Manager(){
        if (!filesystem::exists(note_dir) || !filesystem::is_directory(note_dir)){
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
                course_notes.push_back(make_unique<Course_Notes>(note_dir, course));
            }
        }
    }

    void add_course(const string& course){
        course_notes.push_back(make_unique<Course_Notes>(note_dir, course));
    }

    void list_courses(){
        if (course_notes.empty()){
            cout << "\nERROR: No courses found!\n";
        } else {
            cout << "\nFound current courses:\n";
            for (size_t i = 0; i < course_notes.size(); i++)
                print_centered_left_aligned(to_string(i + 1) + "- " + course_notes[i]->course);
        }

        cout << "\n\nPress any key to continue...";
        cin.ignore();
        cin.get();
    }

    void delete_course(const string& course){
        string course_note_dir = note_dir + "/" + course;
        if (filesystem::exists(course_note_dir) && filesystem::is_directory(course_note_dir)){
            filesystem::remove_all(course_note_dir);
            cout << "\nDirectory \"" << course_note_dir << "\" deleted successfully.\n";
        }

        auto it = remove_if(course_notes.begin(), course_notes.end(), [&](const unique_ptr<Course_Notes>& cn) {
            return cn->course == course;
        });
        course_notes.erase(it, course_notes.end());
    }

    void select_course(){
        list_courses();
        if (course_notes.empty()) return;

        int choice;
        cout << "\nSelect a course (1-" << course_notes.size() << "): ";
        cin >> choice;

        if (choice < 1 || choice > (int)course_notes.size()){
            cout << "Invalid choice.\n";
            return;
        }

        course_notes[choice - 1]->show_notes();
    }
};

int main(int argc, char** argv){
    Note_Manager note_manager;
    int usr_input = 0;

    do {
        system("clear");
        print_centered("=== NOTES MANAGER ===\n");
        print_centered("Following are the available options:\n");
        print_centered_left_aligned("1- Select a course");
        print_centered_left_aligned("2- Add a course");
        print_centered_left_aligned("3- Delete a course");
        print_centered_left_aligned("4- Exit");

        cout << "\nYour choice: (1-4): ";
        cin >> usr_input;

        switch (usr_input){
            case 1: note_manager.select_course(); break;
            case 2: {
                string course;
                cout << "Enter course name: ";
                cin >> course;
                note_manager.add_course(course);
            } break;
            case 3: {
                string course;
                cout << "Enter course name to delete: ";
                cin >> course;
                note_manager.delete_course(course);
            } break;
        }

    } while (usr_input != 4);

    return 0;
}
