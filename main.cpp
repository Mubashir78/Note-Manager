#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
using namespace std;

int get_console_width(){
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    return w.ws_col;
}

void print_centered(const string& text){
    int spaces = (get_console_width() - text.length()) / 2;
    
    cout << setw(spaces) << "" << text << endl;
}

void print_centered_left_aligned(const string& text){
    int spaces = (get_console_width()) / 2.5;
    
    cout << setw(spaces) << "" << text << endl;
}

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

            for (int i=0; i < course_notes.size(); i++)
                print_centered_left_aligned(to_string(i+1) + "- " + course_notes[i]->course);
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

int main(int argc, char** argv){
    Note_Manager note_manager;
    int usr_input;
    
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
            case 1: {
                note_manager.list_course();
            } break;
        }

    } while (usr_input != 4);

    return 0;
}
