#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iomanip>
#include <sqlite3.h>
#include <optional>


using namespace std;


/* ---------------------------
   Utility RAII wrappers
   --------------------------- */
struct DB {
    sqlite3* db = nullptr;
    DB(const string& path) {
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            cerr << "Cannot open DB: " << sqlite3_errmsg(db) << "\n";
            sqlite3_close(db); db = nullptr;
            throw runtime_error("DB open failed");
        }
        // enable foreign keys
        sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    }
    ~DB() { if (db) sqlite3_close(db); }
};

struct Stmt {
    sqlite3_stmt* stmt = nullptr;
    Stmt(sqlite3* db, const string& sql) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw runtime_error(string("Prepare failed: ") + sqlite3_errmsg(db));
        }
    }
    ~Stmt() { if (stmt) sqlite3_finalize(stmt); }
    // convenience
    void reset() { if (stmt) sqlite3_reset(stmt); }
};

/* ---------------------------
   Domain models
   --------------------------- */
struct UserModel {
    int id = -1;
    string username;
    string name;
    string role;
};

struct StudentModel {
    int id = -1;
    int user_id = -1;
    string major;
    int year = 0;
};

struct CourseModel {
    int id = -1;
    string code;
    string title;
    int professor_id = -1;
};

struct FeeModel {
    int id = -1;
    int student_id = -1;
    double amount = 0.0;
    bool paid = false;
};

struct AttendanceModel {
    int id = -1;
    int course_id = -1;
    int student_id = -1;
    bool present = false;
};

struct GradeModel {
    int id = -1;
    int course_id = -1;
    int student_id = -1;
    double grade = 0.0;
};

/* ---------------------------
   Repositories (DAO)
   --------------------------- */
class UserRepo {
    sqlite3* db;
public:
    UserRepo(sqlite3* db_) : db(db_) {}
    sqlite3* get_db() const { return db; }
    void init_schema() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT UNIQUE, password TEXT, role TEXT, name TEXT);";
        char* err = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }
    }
    void seed_accounts() {
        const char* sql = "INSERT OR IGNORE INTO users(username,password,role,name) VALUES"
            "('admin','admin','admin','System Admin'),"
            "('prof_john','pass123','professor','John Smith'),"
            "('student_ali','s123','student','Ali Hassan');";
        char* err = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }
    }
    optional<UserModel> find_by_credentials(const string& username, const string& password) {
        const char* sql = "SELECT id,username,name,role FROM users WHERE username=? AND password=?;";
        Stmt s(db, sql);
        sqlite3_bind_text(s.stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s.stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(s.stmt) == SQLITE_ROW) {
            UserModel u;
            u.id = sqlite3_column_int(s.stmt, 0);
            u.username = (const char*)sqlite3_column_text(s.stmt, 1);
            u.name = (const char*)sqlite3_column_text(s.stmt, 2);
            u.role = (const char*)sqlite3_column_text(s.stmt, 3);
            return u;
        }
        return nullopt;
    }
    bool create_user(const string& username, const string& password, const string& role, const string& name) {
        const char* sql = "INSERT INTO users(username,password,role,name) VALUES(?,?,?,?);";
        Stmt s(db, sql);
        sqlite3_bind_text(s.stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s.stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s.stmt, 3, role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s.stmt, 4, name.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(s.stmt);
        return rc == SQLITE_DONE;
    }
};

class StudentRepo {
    sqlite3* db;
public:
    StudentRepo(sqlite3* db_) : db(db_) {}
    sqlite3* get_db() const { return db; }
    void init_schema() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS students(id INTEGER PRIMARY KEY, user_id INTEGER UNIQUE, major TEXT, year INTEGER, FOREIGN KEY(user_id) REFERENCES users(id));";
        char* err = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }
    }
    void ensure_student_for_username(const string& username, const string& major = "Undeclared", int year = 1) {
        const char* sql =
            "INSERT OR IGNORE INTO students(user_id,major,year) "
            "SELECT u.id,?,? FROM users u WHERE u.username=? AND NOT EXISTS(SELECT 1 FROM students s WHERE s.user_id=u.id);";
        Stmt s(db, sql);
        sqlite3_bind_text(s.stmt, 1, major.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(s.stmt, 2, year);
        sqlite3_bind_text(s.stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(s.stmt);
    }
    vector<pair<StudentModel, UserModel>> list_all_students() {
        const char* sql = "SELECT s.id,s.user_id,s.major,s.year,u.id,u.username,u.name FROM students s JOIN users u ON s.user_id=u.id;";
        Stmt s(db, sql);
        vector<pair<StudentModel, UserModel>> out;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            StudentModel sm; UserModel um;
            sm.id = sqlite3_column_int(s.stmt, 0);
            sm.user_id = sqlite3_column_int(s.stmt, 1);
            sm.major = (const char*)sqlite3_column_text(s.stmt, 2);
            sm.year = sqlite3_column_int(s.stmt, 3);
            um.id = sqlite3_column_int(s.stmt, 4);
            um.username = (const char*)sqlite3_column_text(s.stmt, 5);
            um.name = (const char*)sqlite3_column_text(s.stmt, 6);
            out.push_back({ sm, um });
        }
        return out;
    }
    optional<StudentModel> find_by_user_id(int user_id) {
        const char* sql = "SELECT id,user_id,major,year FROM students WHERE user_id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, user_id);
        if (sqlite3_step(s.stmt) == SQLITE_ROW) {
            StudentModel sm;
            sm.id = sqlite3_column_int(s.stmt, 0);
            sm.user_id = sqlite3_column_int(s.stmt, 1);
            sm.major = (const char*)sqlite3_column_text(s.stmt, 2);
            sm.year = sqlite3_column_int(s.stmt, 3);
            return sm;
        }
        return nullopt;
    }
    optional<StudentModel> find_by_id(int id) {
        const char* sql = "SELECT id,user_id,major,year FROM students WHERE id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, id);
        if (sqlite3_step(s.stmt) == SQLITE_ROW) {
            StudentModel sm;
            sm.id = sqlite3_column_int(s.stmt, 0);
            sm.user_id = sqlite3_column_int(s.stmt, 1);
            sm.major = (const char*)sqlite3_column_text(s.stmt, 2);
            sm.year = sqlite3_column_int(s.stmt, 3);
            return sm;
        }
        return nullopt;
    }
    int create_student_for_user(int user_id, const string& major, int year) {
        const char* sql = "INSERT INTO students(user_id,major,year) VALUES(?,?,?);";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, user_id);
        sqlite3_bind_text(s.stmt, 2, major.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(s.stmt, 3, year);
        sqlite3_step(s.stmt);
        return (int)sqlite3_last_insert_rowid(db);
    }
};

class CourseRepo {
    sqlite3* db;
public:
    CourseRepo(sqlite3* db_) : db(db_) {}
    sqlite3* get_db() const { return db; }
    void init_schema() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS courses(id INTEGER PRIMARY KEY, code TEXT, title TEXT, professor_id INTEGER, FOREIGN KEY(professor_id) REFERENCES users(id));";
        char* err = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }
    }
    vector<CourseModel> list_by_professor(int prof_id) {
        const char* sql = "SELECT id,code,title,professor_id FROM courses WHERE professor_id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, prof_id);
        vector<CourseModel> out;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            CourseModel c;
            c.id = sqlite3_column_int(s.stmt, 0);
            c.code = (const char*)sqlite3_column_text(s.stmt, 1);
            c.title = (const char*)sqlite3_column_text(s.stmt, 2);
            c.professor_id = sqlite3_column_int(s.stmt, 3);
            out.push_back(c);
        }
        return out;
    }
    vector<CourseModel> list_all() {
        const char* sql = "SELECT id,code,title,professor_id FROM courses;";
        Stmt s(db, sql);
        vector<CourseModel> out;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            CourseModel c;
            c.id = sqlite3_column_int(s.stmt, 0);
            c.code = (const char*)sqlite3_column_text(s.stmt, 1);
            c.title = (const char*)sqlite3_column_text(s.stmt, 2);
            c.professor_id = sqlite3_column_int(s.stmt, 3);
            out.push_back(c);
        }
        return out;
    }
    int create_course(const string& code, const string& title, int prof_id) {
        const char* sql = "INSERT INTO courses(code,title,professor_id) VALUES(?,?,?);";
        Stmt s(db, sql);
        sqlite3_bind_text(s.stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s.stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(s.stmt, 3, prof_id);
        sqlite3_step(s.stmt);
        return (int)sqlite3_last_insert_rowid(db);
    }
};

class FeeRepo {
    sqlite3* db;
public:
    FeeRepo(sqlite3* db_) : db(db_) {}
    sqlite3* get_db() const { return db; }
    void init_schema() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS fees(id INTEGER PRIMARY KEY, student_id INTEGER, amount REAL, paid INTEGER, FOREIGN KEY(student_id) REFERENCES students(id));";
        char* err = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }
    }
    void add_fee(int student_id, double amount, bool paid) {
        const char* sql = "INSERT INTO fees(student_id,amount,paid) VALUES(?,?,?);";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, student_id);
        sqlite3_bind_double(s.stmt, 2, amount);
        sqlite3_bind_int(s.stmt, 3, paid ? 1 : 0);
        sqlite3_step(s.stmt);
    }
    vector<FeeModel> list_by_student(int student_id) {
        const char* sql = "SELECT id,student_id,amount,paid FROM fees WHERE student_id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, student_id);
        vector<FeeModel> out;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            FeeModel f;
            f.id = sqlite3_column_int(s.stmt, 0);
            f.student_id = sqlite3_column_int(s.stmt, 1);
            f.amount = sqlite3_column_double(s.stmt, 2);
            f.paid = sqlite3_column_int(s.stmt, 3) != 0;
            out.push_back(f);
        }
        return out;
    }
};

class AttendanceRepo {
    sqlite3* db;
public:
    AttendanceRepo(sqlite3* db_) : db(db_) {}
    void init_schema() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS attendance(id INTEGER PRIMARY KEY, course_id INTEGER, student_id INTEGER, present INTEGER, "
            "FOREIGN KEY(course_id) REFERENCES courses(id), FOREIGN KEY(student_id) REFERENCES students(id));";
        char* err = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }
    }
    vector<AttendanceModel> list_for_course(int course_id) {
        const char* sql = "SELECT id,course_id,student_id,present FROM attendance WHERE course_id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, course_id);
        vector<AttendanceModel> out;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            AttendanceModel a;
            a.id = sqlite3_column_int(s.stmt, 0);
            a.course_id = sqlite3_column_int(s.stmt, 1);
            a.student_id = sqlite3_column_int(s.stmt, 2);
            a.present = sqlite3_column_int(s.stmt, 3) != 0;
            out.push_back(a);
        }
        return out;
    }
    void set_attendance(int course_id, int student_id, bool present) {
        // delete existing then insert (portable)
        const char* del = "DELETE FROM attendance WHERE course_id=? AND student_id=?;";
        Stmt sd(db, del);
        sqlite3_bind_int(sd.stmt, 1, course_id);
        sqlite3_bind_int(sd.stmt, 2, student_id);
        sqlite3_step(sd.stmt);
        const char* ins = "INSERT INTO attendance(course_id,student_id,present) VALUES(?,?,?);";
        Stmt si(db, ins);
        sqlite3_bind_int(si.stmt, 1, course_id);
        sqlite3_bind_int(si.stmt, 2, student_id);
        sqlite3_bind_int(si.stmt, 3, present ? 1 : 0);
        sqlite3_step(si.stmt);
    }
    optional<AttendanceModel> find(int course_id, int student_id) {
        const char* sql = "SELECT id,course_id,student_id,present FROM attendance WHERE course_id=? AND student_id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, course_id);
        sqlite3_bind_int(s.stmt, 2, student_id);
        if (sqlite3_step(s.stmt) == SQLITE_ROW) {
            AttendanceModel a;
            a.id = sqlite3_column_int(s.stmt, 0);
            a.course_id = sqlite3_column_int(s.stmt, 1);
            a.student_id = sqlite3_column_int(s.stmt, 2);
            a.present = sqlite3_column_int(s.stmt, 3) != 0;
            return a;
        }
        return nullopt;
    }
};

class GradeRepo {
    sqlite3* db;
public:
    GradeRepo(sqlite3* db_) : db(db_) {}
    sqlite3* get_db() const { return db; }
    void init_schema() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS grades(id INTEGER PRIMARY KEY, course_id INTEGER, student_id INTEGER, grade REAL, "
            "FOREIGN KEY(course_id) REFERENCES courses(id), FOREIGN KEY(student_id) REFERENCES students(id));";
        char* err = nullptr;
        sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }
    }
    void set_grade(int course_id, int student_id, double grade) {
        const char* del = "DELETE FROM grades WHERE course_id=? AND student_id=?;";
        Stmt sd(db, del);
        sqlite3_bind_int(sd.stmt, 1, course_id);
        sqlite3_bind_int(sd.stmt, 2, student_id);
        sqlite3_step(sd.stmt);
        const char* ins = "INSERT INTO grades(course_id,student_id,grade) VALUES(?,?,?);";
        Stmt si(db, ins);
        sqlite3_bind_int(si.stmt, 1, course_id);
        sqlite3_bind_int(si.stmt, 2, student_id);
        sqlite3_bind_double(si.stmt, 3, grade);
        sqlite3_step(si.stmt);
    }
    vector<GradeModel> list_for_course(int course_id) {
        const char* sql = "SELECT id,course_id,student_id,grade FROM grades WHERE course_id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, course_id);
        vector<GradeModel> out;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            GradeModel g;
            g.id = sqlite3_column_int(s.stmt, 0);
            g.course_id = sqlite3_column_int(s.stmt, 1);
            g.student_id = sqlite3_column_int(s.stmt, 2);
            g.grade = sqlite3_column_double(s.stmt, 3);
            out.push_back(g);
        }
        return out;
    }
    double average_for_student(int student_id) {
        const char* sql = "SELECT AVG(grade) FROM grades WHERE student_id=?;";
        Stmt s(db, sql);
        sqlite3_bind_int(s.stmt, 1, student_id);
        if (sqlite3_step(s.stmt) == SQLITE_ROW) {
            if (sqlite3_column_type(s.stmt, 0) == SQLITE_NULL) return -1.0;
            return sqlite3_column_double(s.stmt, 0);
        }
        return -1.0;
    }
};

/* ---------------------------
   Services
   --------------------------- */
class AuthService {
    UserRepo& userRepo;
public:
    AuthService(UserRepo& ur) : userRepo(ur) {}
    optional<UserModel> login(const string& username, const string& password) {
        return userRepo.find_by_credentials(username, password);
    }
};

class AdminService {
    UserRepo& userRepo;
    StudentRepo& studentRepo;
    FeeRepo& feeRepo;
public:
    AdminService(UserRepo& u, StudentRepo& s, FeeRepo& f) : userRepo(u), studentRepo(s), feeRepo(f) {}
    void create_user(const string& username, const string& password, const string& role, const string& name) {
        if (!userRepo.create_user(username, password, role, name)) {
            cout << "Failed to create user (maybe username exists)\n";
            return;
        }
        if (role == "student") {
            // create student record
            // find user id
            // simple query:
            const char* sql = "SELECT id FROM users WHERE username=?;";
            Stmt st(userRepo.get_db(), sql);
            sqlite3_bind_text(st.stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st.stmt) == SQLITE_ROW) {
                int uid = sqlite3_column_int(st.stmt, 0);
                studentRepo.create_student_for_user(uid, "Undeclared", 1);
            }
        }
        cout << "User created\n";
    }
    void list_students() {
        auto list = studentRepo.list_all_students();
        cout << left << setw(6) << "SID" << setw(15) << "Username" << setw(25) << "Name" << setw(15) << "Major" << setw(6) << "Year" << "\n";
        for (auto& p : list) {
            cout << setw(6) << p.first.id << setw(15) << p.second.username << setw(25) << p.second.name << setw(15) << p.first.major << setw(6) << p.first.year << "\n";
        }
    }
    void add_fee(int student_id, double amount, bool paid) {
        feeRepo.add_fee(student_id, amount, paid);
        cout << "Fee added\n";
    }
};

class ProfessorService {
    CourseRepo& courseRepo;
    AttendanceRepo& attendanceRepo;
    GradeRepo& gradeRepo;
    StudentRepo& studentRepo;
public:
    ProfessorService(CourseRepo& c, AttendanceRepo& a, GradeRepo& g, StudentRepo& s)
        : courseRepo(c), attendanceRepo(a), gradeRepo(g), studentRepo(s) {
    }
    void list_courses(int prof_id) {
        auto courses = courseRepo.list_by_professor(prof_id);
        cout << "Courses:\n";
        for (auto& c : courses) {
            cout << c.id << " | " << c.code << " - " << c.title << "\n";
        }
    }
    void add_course(int prof_id, const string& code, const string& title) {
        courseRepo.create_course(code, title, prof_id);
        cout << "Course added\n";
    }
    void manage_attendance(int course_id) {
        // list students
        const char* sql = "SELECT s.id,u.name FROM students s JOIN users u ON s.user_id=u.id;";
        Stmt s(courseRepo.get_db(), sql);
        vector<pair<int, string>> studs;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            studs.push_back({ sqlite3_column_int(s.stmt,0), (const char*)sqlite3_column_text(s.stmt,1) });
        }
        cout << left << setw(6) << "SID" << setw(25) << "Name" << setw(8) << "Present\n";
        for (auto& st : studs) {
            auto a = attendanceRepo.find(course_id, st.first);
            cout << setw(6) << st.first << setw(25) << st.second << setw(8) << (a && a->present ? 1 : 0) << "\n";
        }
        cout << "Enter student id to toggle/set (0 to back): ";
        int sid; cin >> sid;
        if (sid == 0) return;
        cout << "Present? 1=yes 0=no: "; int p; cin >> p;
        attendanceRepo.set_attendance(course_id, sid, p == 1);
        cout << "Attendance updated\n";
    }
    void manage_grades(int course_id) {
        // list students
        const char* sql = "SELECT s.id,u.name FROM students s JOIN users u ON s.user_id=u.id;";
        Stmt s(courseRepo.get_db(), sql);
        vector<pair<int, string>> studs;
        while (sqlite3_step(s.stmt) == SQLITE_ROW) {
            studs.push_back({ sqlite3_column_int(s.stmt,0), (const char*)sqlite3_column_text(s.stmt,1) });
        }
        cout << left << setw(6) << "SID" << setw(25) << "Name" << setw(8) << "Grade\n";
        for (auto& st : studs) {
            // try to find grade
            const char* gsql = "SELECT grade FROM grades WHERE course_id=? AND student_id=?;";
            Stmt gs(courseRepo.get_db(), gsql);
            sqlite3_bind_int(gs.stmt, 1, course_id);
            sqlite3_bind_int(gs.stmt, 2, st.first);
            if (sqlite3_step(gs.stmt) == SQLITE_ROW) {
                cout << setw(6) << st.first << setw(25) << st.second << setw(8) << sqlite3_column_double(gs.stmt, 0) << "\n";
            }
            else {
                cout << setw(6) << st.first << setw(25) << st.second << setw(8) << "N/A" << "\n";
            }
        }
        cout << "Enter student id to set grade (0 to back): ";
        int sid; cin >> sid;
        if (sid == 0) return;
        cout << "Grade (0-100): "; double grade; cin >> grade;
        gradeRepo.set_grade(course_id, sid, grade);
        cout << "Grade saved\n";
    }
};

class StudentService {
    StudentRepo& studentRepo;
    CourseRepo& courseRepo;
    AttendanceRepo& attendanceRepo;
    GradeRepo& gradeRepo;
    FeeRepo& feeRepo;
public:
    StudentService(StudentRepo& s, CourseRepo& c, AttendanceRepo& a, GradeRepo& g, FeeRepo& f)
        : studentRepo(s), courseRepo(c), attendanceRepo(a), gradeRepo(g), feeRepo(f) {
    }
    void show_profile(int user_id) {
        auto s = studentRepo.find_by_user_id(user_id);
        if (!s) { cout << "Student record not found\n"; return; }
        // get username/name
        const char* sql = "SELECT username,name FROM users WHERE id=?;";
        Stmt st(studentRepo.get_db(), sql);
        sqlite3_bind_int(st.stmt, 1, user_id);
        if (sqlite3_step(st.stmt) == SQLITE_ROW) {
            cout << "Username: " << (const char*)sqlite3_column_text(st.stmt, 0) << "\n";
            cout << "Name: " << (const char*)sqlite3_column_text(st.stmt, 1) << "\n";
        }
        cout << "Major: " << s->major << "  Year: " << s->year << "\n";
    }
    void list_courses() {
        auto cs = courseRepo.list_all();
        cout << "Courses available:\n";
        for (auto& c : cs) cout << c.id << " | " << c.code << " - " << c.title << "\n";
    }
    void view_attendance(int user_id) {
        auto s = studentRepo.find_by_user_id(user_id);
        if (!s) { cout << "Student record not found\n"; return; }
        const char* sql = "SELECT c.code,c.title,IFNULL(a.present,0) FROM courses c LEFT JOIN attendance a ON a.course_id=c.id AND a.student_id=?;";
        Stmt st(studentRepo.get_db(), sql);
        sqlite3_bind_int(st.stmt, 1, s->id);
        cout << left << setw(10) << "Code" << setw(30) << "Title" << setw(8) << "Present\n";
        while (sqlite3_step(st.stmt) == SQLITE_ROW) {
            cout << setw(10) << (const char*)sqlite3_column_text(st.stmt, 0)
                << setw(30) << (const char*)sqlite3_column_text(st.stmt, 1)
                << setw(8) << sqlite3_column_int(st.stmt, 2) << "\n";
        }
    }
    void view_gpa(int user_id) {
        auto s = studentRepo.find_by_user_id(user_id);
        if (!s) { cout << "Student record not found\n"; return; }
        double avg = gradeRepo.average_for_student(s->id);
        if (avg < 0) cout << "No grades yet\n"; else cout << "GPA (avg grade): " << fixed << setprecision(2) << avg << "\n";
    }
    void view_fees(int user_id) {
        auto s = studentRepo.find_by_user_id(user_id);
        if (!s) { cout << "Student record not found\n"; return; }
        auto fees = feeRepo.list_by_student(s->id);
        cout << left << setw(6) << "FID" << setw(12) << "Amount" << setw(8) << "Paid\n";
        for (auto& f : fees) cout << setw(6) << f.id << setw(12) << f.amount << setw(8) << (f.paid ? "Yes" : "No") << "\n";
    }
};

/* ---------------------------
   CLI / Application
   --------------------------- */
class App {
    unique_ptr<DB> db;
    unique_ptr<UserRepo> userRepo;
    unique_ptr<StudentRepo> studentRepo;
    unique_ptr<CourseRepo> courseRepo;
    unique_ptr<FeeRepo> feeRepo;
    unique_ptr<AttendanceRepo> attendanceRepo;
    unique_ptr<GradeRepo> gradeRepo;

    unique_ptr<AuthService> authService;
    unique_ptr<AdminService> adminService;
    unique_ptr<ProfessorService> professorService;
    unique_ptr<StudentService> studentService;

public:
    App(const string& dbpath = "university.db") {
        db = make_unique<DB>(dbpath);
        userRepo = make_unique<UserRepo>(db->db);
        studentRepo = make_unique<StudentRepo>(db->db);
        courseRepo = make_unique<CourseRepo>(db->db);
        feeRepo = make_unique<FeeRepo>(db->db);
        attendanceRepo = make_unique<AttendanceRepo>(db->db);
        gradeRepo = make_unique<GradeRepo>(db->db);

        // init schemas
        userRepo->init_schema();
        studentRepo->init_schema();
        courseRepo->init_schema();
        feeRepo->init_schema();
        attendanceRepo->init_schema();
        gradeRepo->init_schema();

        // seed
        userRepo->seed_accounts();
        studentRepo->ensure_student_for_username("student_ali", "Computer Science", 2);
        // seed a course for professor
        const char* sql = "INSERT OR IGNORE INTO courses(code,title,professor_id) "
            "SELECT 'CS101','Intro to Programming',u.id FROM users u WHERE u.username='prof_john' AND NOT EXISTS(SELECT 1 FROM courses c WHERE c.code='CS101');";
        char* err = nullptr;
        sqlite3_exec(db->db, sql, nullptr, nullptr, &err);
        if (err) { string e = err; sqlite3_free(err); throw runtime_error(e); }

        // services
        authService = make_unique<AuthService>(*userRepo);
        adminService = make_unique<AdminService>(*userRepo, *studentRepo, *feeRepo);
        professorService = make_unique<ProfessorService>(*courseRepo, *attendanceRepo, *gradeRepo, *studentRepo);
        studentService = make_unique<StudentService>(*studentRepo, *courseRepo, *attendanceRepo, *gradeRepo, *feeRepo);
    }

    void run() {
        cout << "Welcome to OOP UMS CLI\n";
        string username, password;
        cout << "Username: "; cin >> username;
        cout << "Password: "; cin >> password;
        auto userOpt = authService->login(username, password);
        if (!userOpt) { cout << "Login failed\n"; return; }
        UserModel user = *userOpt;
        cout << "Hello, " << user.name << " (" << user.role << ")\n";
        if (user.role == "admin") admin_loop(user);
        else if (user.role == "professor") professor_loop(user);
        else if (user.role == "student") student_loop(user);
        else cout << "Unknown role\n";
    }

private:
    void admin_loop(const UserModel& user) {
        while (true) {
            cout << "\nAdmin Menu: 1)Profile 2)Students 3)Fees 4)Create User 0)Logout\nChoice: ";
            int c; cin >> c;
            if (c == 0) break;
            if (c == 1) {
                cout << "Admin Profile\nUsername: " << user.username << "\nName: " << user.name << "\n";
            }
            else if (c == 2) {
                adminService->list_students();
                cout << "1)Add student 0)Back\nChoice: "; int ch; cin >> ch;
                if (ch == 1) {
                    string uname, pwd, name, major; int year;
                    cout << "Username: "; cin >> uname;
                    cout << "Password: "; cin >> pwd;
                    cin.ignore(); cout << "Name: "; getline(cin, name);
                    cout << "Major: "; getline(cin, major);
                    cout << "Year: "; cin >> year;
                    if (userRepo->create_user(uname, pwd, "student", name)) {
                        // create student record
                        const char* sql = "SELECT id FROM users WHERE username=?;";
                        Stmt s(userRepo->get_db(), sql);
                        sqlite3_bind_text(s.stmt, 1, uname.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(s.stmt) == SQLITE_ROW) {
                            int uid = sqlite3_column_int(s.stmt, 0);
                            studentRepo->create_student_for_user(uid, major, year);
                            cout << "Student created\n";
                        }
                    }
                    else cout << "Failed to create user\n";
                }
            }
            else if (c == 3) {
                cout << "Enter student id to add fee: "; int sid; cin >> sid;
                cout << "Amount: "; double amt; cin >> amt;
                cout << "Paid? 1=yes 0=no: "; int p; cin >> p;
                adminService->add_fee(sid, amt, p == 1);
            }
            else if (c == 4) {
                string uname, pwd, name, role;
                cout << "Username: "; cin >> uname;
                cout << "Password: "; cin >> pwd;
                cin.ignore(); cout << "Full name: "; getline(cin, name);
                cout << "Role (admin/professor/student): "; cin >> role;
                if (!userRepo->create_user(uname, pwd, role, name)) cout << "Create failed\n";
                else {
                    if (role == "student") {
                        const char* sql = "SELECT id FROM users WHERE username=?;";
                        Stmt s(userRepo->get_db(), sql);
                        sqlite3_bind_text(s.stmt, 1, uname.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(s.stmt) == SQLITE_ROW) {
                            int uid = sqlite3_column_int(s.stmt, 0);
                            studentRepo->create_student_for_user(uid, "Undeclared", 1);
                        }
                    }
                    cout << "User created\n";
                }
            }
        }
    }

    void professor_loop(const UserModel& user) {
        while (true) {
            cout << "\nProfessor Menu: 1)Profile 2)Courses 3)Attendance 4)Grades 0)Logout\nChoice: ";
            int c; cin >> c;
            if (c == 0) break;
            if (c == 1) {
                cout << "Professor Profile\nUsername: " << user.username << "\nName: " << user.name << "\n";
            }
            else if (c == 2) {
                professorService->list_courses(user.id);
                cout << "1)Add course 0)Back\nChoice: "; int ch; cin >> ch;
                if (ch == 1) {
                    string code, title; cin.ignore(); cout << "Code: "; getline(cin, code); cout << "Title: "; getline(cin, title);
                    professorService->add_course(user.id, code, title);
                }
            }
            else if (c == 3) {
                professorService->list_courses(user.id);
                cout << "Enter course id to manage attendance (0 back): "; int cid; cin >> cid;
                if (cid != 0) professorService->manage_attendance(cid);
            }
            else if (c == 4) {
                professorService->list_courses(user.id);
                cout << "Enter course id to manage grades (0 back): "; int cid; cin >> cid;
                if (cid != 0) professorService->manage_grades(cid);
            }
        }
    }

    void student_loop(const UserModel& user) {
        while (true) {
            cout << "\nStudent Menu: 1)Profile 2)Courses 3)Attendance 4)GPA 5)Fees 0)Logout\nChoice: ";
            int c; cin >> c;
            if (c == 0) break;
            if (c == 1) studentService->show_profile(user.id);
            else if (c == 2) studentService->list_courses();
            else if (c == 3) studentService->view_attendance(user.id);
            else if (c == 4) studentService->view_gpa(user.id);
            else if (c == 5) studentService->view_fees(user.id);
        }
    }
};

/* ---------------------------
   main
   --------------------------- */
int main() {
    try {
        App app;
        app.run();
    }
    catch (const exception& ex) {
        cerr << "Fatal: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
