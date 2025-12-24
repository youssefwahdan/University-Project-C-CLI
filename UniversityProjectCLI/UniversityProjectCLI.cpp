// main.cpp - minimal CLI University Management with SQLite
#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>

using namespace std;

static int exec_cb(void*, int argc, char** argv, char** col) {
    for (int i = 0;i < argc;i++) cout << (col[i] ? col[i] : "") << ": " << (argv[i] ? argv[i] : "") << "\n";
    cout << "-----------------\n"; return 0;
}

void check(int rc, char* err) {
    if (rc != SQLITE_OK) { cerr << "SQL error: " << (err ? err : "") << "\n"; sqlite3_free(err); exit(1); }
}

void init_db(sqlite3* db) {
    char* err = 0;
    string sql =
        "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT UNIQUE, password TEXT, role TEXT, name TEXT);"
        "CREATE TABLE IF NOT EXISTS students(id INTEGER PRIMARY KEY, user_id INTEGER, major TEXT, year INTEGER);"
        "CREATE TABLE IF NOT EXISTS fees(id INTEGER PRIMARY KEY, student_id INTEGER, amount REAL, paid INTEGER);"
        "CREATE TABLE IF NOT EXISTS courses(id INTEGER PRIMARY KEY, code TEXT, title TEXT, professor_id INTEGER);"
        "CREATE TABLE IF NOT EXISTS attendance(id INTEGER PRIMARY KEY, course_id INTEGER, student_id INTEGER, present INTEGER);"
        "CREATE TABLE IF NOT EXISTS grades(id INTEGER PRIMARY KEY, course_id INTEGER, student_id INTEGER, grade REAL);";
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err);
    check(rc, err);
    // seed admin if not exists
    sql = "INSERT OR IGNORE INTO users(username,password,role,name) VALUES('admin','admin','admin','System Admin');";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err); check(rc, err);
}

bool login(sqlite3* db, string& role, int& user_id, string& name) {
    cout << "Username: "; string u, p; cin >> u;
    cout << "Password: "; cin >> p;
    string sql = "SELECT id,role,name FROM users WHERE username=? AND password=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, u.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, p.c_str(), -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
        role = (const char*)sqlite3_column_text(stmt, 1);
        name = (const char*)sqlite3_column_text(stmt, 2);
        sqlite3_finalize(stmt);
        return true;
    }
    sqlite3_finalize(stmt);
    cout << "Login failed\n"; return false;
}

// Admin functions
void admin_profile(sqlite3* db, int uid) {
    cout << "Admin profile\n";
    string sql = "SELECT username,name FROM users WHERE id=?;";
    sqlite3_stmt* s; sqlite3_prepare_v2(db, sql.c_str(), -1, &s, 0);
    sqlite3_bind_int(s, 1, uid);
    if (sqlite3_step(s) == SQLITE_ROW) {
        cout << "Username: " << (const char*)sqlite3_column_text(s, 0) << "\n";
        cout << "Name: " << (const char*)sqlite3_column_text(s, 1) << "\n";
    }
    sqlite3_finalize(s);
}

void admin_add_student(sqlite3* db) {
    string uname, pwd, name; string major; int year;
    cout << "New student username: "; cin >> uname;
    cout << "Password: "; cin >> pwd;
    cout << "Name: "; cin.ignore(); getline(cin, name);
    cout << "Major: "; getline(cin, major);
    cout << "Year: "; cin >> year;
    string sql = "INSERT INTO users(username,password,role,name) VALUES(?,?, 'student',?);";
    sqlite3_stmt* s; sqlite3_prepare_v2(db, sql.c_str(), -1, &s, 0);
    sqlite3_bind_text(s, 1, uname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, pwd.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 3, name.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(s) != SQLITE_DONE) cerr << "Error creating user\n";
    sqlite3_finalize(s);
    int uid = sqlite3_last_insert_rowid(db);
    sql = "INSERT INTO students(user_id,major,year) VALUES(?,?,?);";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &s, 0);
    sqlite3_bind_int(s, 1, uid); sqlite3_bind_text(s, 2, major.c_str(), -1, SQLITE_STATIC); sqlite3_bind_int(s, 3, year);
    if (sqlite3_step(s) != SQLITE_DONE) cerr << "Error creating student\n";
    sqlite3_finalize(s);
    cout << "Student added\n";
}

void admin_students_tab(sqlite3* db) {
    cout << "Students list:\n";
    string sql = "SELECT s.id,u.username,u.name,s.major,s.year FROM students s JOIN users u ON s.user_id=u.id;";
    char* err = 0; sqlite3_exec(db, sql.c_str(), exec_cb, 0, &err); if (err) check(SQLITE_OK, err);
    cout << "1) Add student  0) Back\nChoice: "; int c; cin >> c;
    if (c == 1) admin_add_student(db);
}

void admin_fees_tab(sqlite3* db) {
    cout << "Fees management\n";
    cout << "Enter student id to add/edit fee: "; int sid; cin >> sid;
    cout << "Amount: "; double amt; cin >> amt;
    cout << "Paid? 1=yes 0=no: "; int paid; cin >> paid;
    string sql = "INSERT INTO fees(student_id,amount,paid) VALUES(?,?,?);";
    sqlite3_stmt* s; sqlite3_prepare_v2(db, sql.c_str(), -1, &s, 0);
    sqlite3_bind_int(s, 1, sid); sqlite3_bind_double(s, 2, amt); sqlite3_bind_int(s, 3, paid);
    if (sqlite3_step(s) != SQLITE_DONE) cerr << "Error\n"; sqlite3_finalize(s);
    cout << "Fee recorded\n";
}

void admin_menu(sqlite3* db, int uid) {
    while (true) {
        cout << "\nAdmin Menu: 1)Profile 2)Students 3)Fees 0)Logout\nChoice: ";
        int c; cin >> c;
        if (c == 1) admin_profile(db, uid);
        else if (c == 2) admin_students_tab(db);
        else if (c == 3) admin_fees_tab(db);
        else break;
    }
}

// Minimal professor and student menus (extend similarly)
void professor_menu(sqlite3* db, int uid) {
    cout << "Professor menu (skeleton). Extend attendance/grades similarly.\n";
}
void student_menu(sqlite3* db, int uid) {
    cout << "Student menu (skeleton). Extend GPA/fees view similarly.\n";
}

int main() {
    sqlite3* db; sqlite3_open("university.db", &db);
    init_db(db);
    string role, name; int uid;
    cout << "Welcome to UMS CLI\n";
    if (!login(db, role, uid, name)) return 0;
    cout << "Hello, " << name << " (" << role << ")\n";
    if (role == "admin") admin_menu(db, uid);
    else if (role == "professor") professor_menu(db, uid);
    else if (role == "student") student_menu(db, uid);
    sqlite3_close(db);
    return 0;
}
