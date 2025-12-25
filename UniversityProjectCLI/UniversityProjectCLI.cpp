#include <iostream>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <ctime>

using namespace std;

// Global database connection
sqlite3* db;

// Helper function to execute SQL queries
bool executeSQL(const char* sql) {
    char* errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// User base class
class User {
protected:
    int id;
    string username;
    string password;
    string name;
    string email;
    string role;
public:
    User(int id, string username, string password, string name, string email, string role)
        : id(id), username(username), password(password), name(name), email(email), role(role) {
    }

    virtual void displayMenu() = 0;
    string getRole() const { return role; }
    int getId() const { return id; }
    string getName() const { return name; }
};

// Student class
class Student : public User {
private:
    int departmentId;
    double feesDue;
    double feesPaid;
public:
    Student(int id, string username, string password, string name, string email, int deptId, double due, double paid)
        : User(id, username, password, name, email, "student"), departmentId(deptId), feesDue(due), feesPaid(paid) {
    }

    void displayMenu() override;
    void showProfile();
    void showAttendance();
    void showFees();
    void showGrades();
};

// Professor class
class Professor : public User {
private:
    vector<int> departmentIds;
    vector<int> courseIds;
public:
    Professor(int id, string username, string password, string name, string email, vector<int> depts, vector<int> courses)
        : User(id, username, password, name, email, "professor"), departmentIds(depts), courseIds(courses) {
    }

    void displayMenu() override;
    void viewProfile();
    void addAttendance();
    void addGrades();
    void showStudents();
};

// Admin class
class Admin : public User {
public:
    Admin(int id, string username, string password, string name, string email)
        : User(id, username, password, name, email, "admin") {
    }

    void displayMenu() override;
    void manageUsers();
    void listUsers();
    void addDepartment();
    void showGrades();
    void showAttendance();
    void addCourse();
    void assignProfessor();
    void manageFees();
};

// Login function
User* login() {
    string username, password;
    cout << "=== University Login ===\n";
    cout << "Username: ";
    cin >> username;
    cout << "Password: ";
    cin >> password;

    // First get basic user info
    string sql = "SELECT id, username, password, name, email, role, department_id "
        "FROM users WHERE username = '" + username + "' AND password = '" + password + "';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
        return nullptr;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        string email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        string role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        int deptId = sqlite3_column_int(stmt, 6);

        sqlite3_finalize(stmt);

        if (role == "admin") {
            return new Admin(id, username, password, name, email);
        }
        else if (role == "student") {
            // Get student-specific data
            string studentSql = "SELECT fees_due, fees_paid FROM students WHERE user_id = " + to_string(id) + ";";
            sqlite3_stmt* studentStmt;
            double due = 0.0, paid = 0.0;

            if (sqlite3_prepare_v2(db, studentSql.c_str(), -1, &studentStmt, 0) == SQLITE_OK &&
                sqlite3_step(studentStmt) == SQLITE_ROW) {
                due = sqlite3_column_double(studentStmt, 0);
                paid = sqlite3_column_double(studentStmt, 1);
            }
            sqlite3_finalize(studentStmt);

            return new Student(id, username, password, name, email, deptId, due, paid);
        }
        else if (role == "professor") {
            vector<int> depts, courses;
            // Get professor departments
            string deptSql = "SELECT department_id FROM professor_departments WHERE professor_id = " + to_string(id) + ";";
            sqlite3_stmt* deptStmt;
            if (sqlite3_prepare_v2(db, deptSql.c_str(), -1, &deptStmt, 0) == SQLITE_OK) {
                while (sqlite3_step(deptStmt) == SQLITE_ROW) {
                    depts.push_back(sqlite3_column_int(deptStmt, 0));
                }
                sqlite3_finalize(deptStmt);
            }

            // Get professor courses
            string courseSql = "SELECT course_id FROM professor_courses WHERE professor_id = " + to_string(id) + ";";
            sqlite3_stmt* courseStmt;
            if (sqlite3_prepare_v2(db, courseSql.c_str(), -1, &courseStmt, 0) == SQLITE_OK) {
                while (sqlite3_step(courseStmt) == SQLITE_ROW) {
                    courses.push_back(sqlite3_column_int(courseStmt, 0));
                }
                sqlite3_finalize(courseStmt);
            }

            return new Professor(id, username, password, name, email, depts, courses);
        }
    }

    sqlite3_finalize(stmt);
    cout << "Invalid credentials!" << endl;
    return nullptr;
}

// Student member functions
void Student::showProfile() {
    cout << "\n=== Student Profile ===\n";
    cout << "Name: " << name << endl;
    cout << "Email: " << email << endl;

    // Get department name
    string sql = "SELECT name FROM departments WHERE id = " + to_string(departmentId) + ";";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        string deptName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        cout << "Department: " << deptName << endl;
        cout << string(23, '-') << endl;
    }
    sqlite3_finalize(stmt);
}

void Student::showAttendance() {
    cout << "\n=== Attendance Records ===\n";
    string sql = "SELECT courses.name, attendance.date, attendance.status "
        "FROM attendance "
        "JOIN courses ON attendance.course_id = courses.id "
        "WHERE student_id = " + to_string(id) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << left << setw(20) << "Course" << setw(15) << "Date" << setw(10) << "Status" << endl;
    cout << string(50, '-') << endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string course = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        string date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        cout << left << setw(20) << course << setw(15) << date << setw(10) << status << endl;
    }
    sqlite3_finalize(stmt);
}

void Student::showFees() {
    cout << "\n=== Fee Details ===\n";
    cout << "Fees Due: $" << fixed << setprecision(2) << feesDue << endl;
    cout << "Fees Paid: $" << fixed << setprecision(2) << feesPaid << endl;
    cout << "Balance: $" << fixed << setprecision(2) << (feesDue - feesPaid) << endl;
    cout << string(19, '-') << endl;
}

void Student::showGrades() {
    cout << "\n=== Grade Report ===\n";
    string sql = "SELECT courses.name, grades.assignment1, grades.assignment2, "
        "grades.coursework, grades.final_exam, grades.total, grades.grade_letter "
        "FROM grades "
        "JOIN courses ON grades.course_id = courses.id "
        "WHERE student_id = " + to_string(id) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << left << setw(15) << "Course" << setw(10) << "Ass1" << setw(10) << "Ass2"
        << setw(10) << "CW" << setw(10) << "Final" << setw(10) << "Total" << setw(10) << "Grade" << endl;
    cout << string(80, '-') << endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string course = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        double ass1 = sqlite3_column_double(stmt, 1);
        double ass2 = sqlite3_column_double(stmt, 2);
        double cw = sqlite3_column_double(stmt, 3);
        double final = sqlite3_column_double(stmt, 4);
        double total = sqlite3_column_double(stmt, 5);
        string grade = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

        cout << left << setw(15) << course << setw(10) << ass1 << setw(10) << ass2 << setw(10) << cw << setw(10) << final << setw(10) << total << setw(10) << grade << endl;
    }
    sqlite3_finalize(stmt);
}

void Student::displayMenu() {
    int choice;
    while (true) {
        cout << "\n=== Student Menu ===\n";
        cout << "1. Show Profile\n";
        cout << "2. Show Attendance\n";
        cout << "3. Show Fees\n";
        cout << "4. Show Grades\n";
        cout << "5. Logout\n";
        cout << "Enter choice: ";
        cin >> choice;

        switch (choice) {
        case 1: showProfile(); break;
        case 2: showAttendance(); break;
        case 3: showFees(); break;
        case 4: showGrades(); break;
        case 5: return;
        default: cout << "Invalid choice!" << endl;
        }
    }
}

// Professor member functions
void Professor::viewProfile() {
    cout << "\n=== Professor Profile ===\n";
    cout << "Name: " << name << endl;
    cout << "Email: " << email << endl;

    // Display departments
    cout << "Departments: ";
    for (size_t i = 0; i < departmentIds.size(); ++i) {
        string sql = "SELECT name FROM departments WHERE id = " + to_string(departmentIds[i]) + ";";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            cout << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (i < departmentIds.size() - 1) cout << ", ";
        }
        sqlite3_finalize(stmt);
    }
    cout << endl;

    // Display courses
    cout << "Courses: ";
    for (size_t i = 0; i < courseIds.size(); ++i) {
        string sql = "SELECT name FROM courses WHERE id = " + to_string(courseIds[i]) + ";";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            cout << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (i < courseIds.size() - 1) cout << ", ";
        }
        sqlite3_finalize(stmt);
    }
    cout << endl;
}

void Professor::addAttendance() {
    if (courseIds.empty()) {
        cout << "You have no courses assigned!" << endl;
        return;
    }

    cout << "\n=== Add Attendance ===\n";
    cout << "Select course:\n";
    for (size_t i = 0; i < courseIds.size(); ++i) {
        string sql = "SELECT name FROM courses WHERE id = " + to_string(courseIds[i]) + ";";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            cout << i + 1 << ". " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) << endl;
        }
        sqlite3_finalize(stmt);
    }

    int choice;
    cout << "Enter choice: ";
    cin >> choice;
    if (choice < 1 || choice > static_cast<int>(courseIds.size())) {
        cout << "Invalid choice!" << endl;
        return;
    }
    int courseId = courseIds[choice - 1];

    // Get students in this course/department
    string sql = "SELECT users.id, users.name FROM users "
        "JOIN students ON users.id = students.user_id "
        "JOIN departments ON students.department_id = departments.id "
        "JOIN courses ON courses.department_id = departments.id "
        "WHERE courses.id = " + to_string(courseId) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    vector<pair<int, string>> students;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int sid = sqlite3_column_int(stmt, 0);
        string sname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        students.push_back({ sid, sname });
    }
    sqlite3_finalize(stmt);

    if (students.empty()) {
        cout << "No students found for this course!" << endl;
        return;
    }

    time_t now = time(nullptr);
    char date[11];
    struct tm timeinfo;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&timeinfo, &now);
#else
    localtime_r(&now, &timeinfo);
#endif
    strftime(date, sizeof(date), "%Y-%m-%d", &timeinfo);

    cout << "\nEnter attendance for " << date << ":\n";
    for (auto& student : students) {
        char status;
        cout << student.second << " (p/a): ";
        cin >> status;
        status = tolower(status);

        if (status == 'p' || status == 'a') {
            string statusStr = (status == 'p') ? "present" : "absent";
            string insertSql = "INSERT INTO attendance (student_id, course_id, date, status) "
                "VALUES (" + to_string(student.first) + ", " + to_string(courseId) + ", '"
                + string(date) + "', '" + statusStr + "');";
            executeSQL(insertSql.c_str());
        }
    }
    cout << "Attendance recorded successfully!" << endl;
}

void Professor::addGrades() {
    if (courseIds.empty()) {
        cout << "You have no courses assigned!" << endl;
        return;
    }

    cout << "\n=== Add Grades ===\n";
    cout << "Select course:\n";
    for (size_t i = 0; i < courseIds.size(); ++i) {
        string sql = "SELECT name, course_type FROM courses WHERE id = " + to_string(courseIds[i]) + ";";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            cout << i + 1 << ". " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))
                << " (" << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) << ")" << endl;
        }
        sqlite3_finalize(stmt);
    }

    int choice;
    cout << "Enter choice: ";
    cin >> choice;
    if (choice < 1 || choice > static_cast<int>(courseIds.size())) {
        cout << "Invalid choice!" << endl;
        return;
    }
    int courseId = courseIds[choice - 1];

    // Get course type
    string courseType;
    string typeSql = "SELECT course_type FROM courses WHERE id = " + to_string(courseId) + ";";
    sqlite3_stmt* typeStmt;
    if (sqlite3_prepare_v2(db, typeSql.c_str(), -1, &typeStmt, 0) == SQLITE_OK && sqlite3_step(typeStmt) == SQLITE_ROW) {
        courseType = reinterpret_cast<const char*>(sqlite3_column_text(typeStmt, 0));
    }
    sqlite3_finalize(typeStmt);

    // Get students
    string sql = "SELECT users.id, users.name FROM users "
        "JOIN students ON users.id = students.user_id "
        "JOIN departments ON students.department_id = departments.id "
        "JOIN courses ON courses.department_id = departments.id "
        "WHERE courses.id = " + to_string(courseId) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    vector<pair<int, string>> students;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int sid = sqlite3_column_int(stmt, 0);
        string sname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        students.push_back({ sid, sname });
    }
    sqlite3_finalize(stmt);

    if (students.empty()) {
        cout << "No students found for this course!" << endl;
        return;
    }

    cout << "\nEnter grades for course (" << courseType << "):\n";
    for (auto& student : students) {
        cout << "\nStudent: " << student.second << endl;
        double ass1, ass2, cw, final;

        cout << "Assignment 1 (20%): ";
        cin >> ass1;
        cout << "Assignment 2 (30%): ";
        cin >> ass2;
        cout << "Coursework (20%): ";
        cin >> cw;

        if (courseType == "theoretical") {
            final = 0; // Not used for theoretical
            cout << "Final Exam (60%): ";
            cin >> final;
        }
        else {
            cout << "Final Exam (30%): ";
            cin >> final;
        }

        // Calculate total
        double total;
        if (courseType == "theoretical") {
            total = (ass1 * 0.2) + (ass2 * 0.2) + (final * 0.6);
        }
        else {
            total = (ass1 * 0.2) + (ass2 * 0.3) + (cw * 0.2) + (final * 0.3);
        }

        // Determine grade letter
        string grade;
        if (total >= 85) grade = "Excellent";
        else if (total >= 75) grade = "Very Good";
        else if (total >= 65) grade = "Good";
        else if (total >= 60) grade = "Pass";
        else grade = "Fail";

        // Check if grade exists
        string checkSql = "SELECT id FROM grades WHERE student_id = " + to_string(student.first)
            + " AND course_id = " + to_string(courseId) + ";";
        sqlite3_stmt* checkStmt;
        bool exists = false;
        if (sqlite3_prepare_v2(db, checkSql.c_str(), -1, &checkStmt, 0) == SQLITE_OK && sqlite3_step(checkStmt) == SQLITE_ROW) {
            exists = true;
        }
        sqlite3_finalize(checkStmt);

        if (exists) {
            string updateSql = "UPDATE grades SET assignment1 = " + to_string(ass1) +
                ", assignment2 = " + to_string(ass2) +
                ", coursework = " + to_string(cw) +
                ", final_exam = " + to_string(final) +
                ", total = " + to_string(total) +
                ", grade_letter = '" + grade + "' " +
                "WHERE student_id = " + to_string(student.first) +
                " AND course_id = " + to_string(courseId) + ";";
            executeSQL(updateSql.c_str());
        }
        else {
            string insertSql = "INSERT INTO grades (student_id, course_id, assignment1, assignment2, coursework, final_exam, total, grade_letter) "
                "VALUES (" + to_string(student.first) + ", " + to_string(courseId) + ", "
                + to_string(ass1) + ", " + to_string(ass2) + ", " + to_string(cw) + ", "
                + to_string(final) + ", " + to_string(total) + ", '" + grade + "');";
            executeSQL(insertSql.c_str());
        }
    }
    cout << "Grades recorded successfully!" << endl;
}

void Professor::showStudents() {
    if (courseIds.empty()) {
        cout << "You have no courses assigned!" << endl;
        return;
    }

    cout << "\n=== Students in Your Courses ===\n";
    cout << "Select course:\n";
    for (size_t i = 0; i < courseIds.size(); ++i) {
        string sql = "SELECT name FROM courses WHERE id = " + to_string(courseIds[i]) + ";";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            cout << i + 1 << ". " << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) << endl;
        }
        sqlite3_finalize(stmt);
    }

    int choice;
    cout << "Enter choice: ";
    cin >> choice;
    if (choice < 1 || choice > static_cast<int>(courseIds.size())) {
        cout << "Invalid choice!" << endl;
        return;
    }
    int courseId = courseIds[choice - 1];

    string sql = "SELECT users.name, students.user_id FROM users "
        "JOIN students ON users.id = students.user_id "
        "JOIN departments ON students.department_id = departments.id "
        "JOIN courses ON courses.department_id = departments.id "
        "WHERE courses.id = " + to_string(courseId) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << "\nStudents enrolled:\n";
    cout << left << setw(20) << "Name" << "Student ID" << endl;
    cout << string(30, '-') << endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string sname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int sid = sqlite3_column_int(stmt, 1);
        cout << left << setw(20) << sname << sid << endl;
    }
    sqlite3_finalize(stmt);
}

void Professor::displayMenu() {
    int choice;
    while (true) {
        cout << "\n=== Professor Menu ===\n";
        cout << "1. View Profile\n";
        cout << "2. Add Attendance\n";
        cout << "3. Add Grades\n";
        cout << "4. Show Students\n";
        cout << "5. Logout\n";
        cout << "Enter choice: ";
        cin >> choice;

        switch (choice) {
        case 1: viewProfile(); break;
        case 2: addAttendance(); break;
        case 3: addGrades(); break;
        case 4: showStudents(); break;
        case 5: return;
        default: cout << "Invalid choice!" << endl;
        }
    }
}

// Admin member functions
void Admin::manageUsers() {
    int choice;
    while (true) {
        cout << "\n=== User Management ===\n";
        cout << "1. Add Student\n";
        cout << "2. Add Professor\n";
        cout << "3. Back\n";
        cout << "Enter choice: ";
        cin >> choice;

        if (choice == 3) break;

        string username, password, name, email;
        cout << "Username: ";
        cin >> username;
        cout << "Password: ";
        cin >> password;
        cout << "Full Name: ";
        cin.ignore();
        getline(cin, name);
        cout << "Email: ";
        cin >> email;

        if (choice == 1) {
            // Get department ID
            cout << "\nAvailable Departments:\n";
            string deptSql = "SELECT id, name FROM departments;";
            sqlite3_stmt* deptStmt;
            if (sqlite3_prepare_v2(db, deptSql.c_str(), -1, &deptStmt, 0) == SQLITE_OK) {
                while (sqlite3_step(deptStmt) == SQLITE_ROW) {
                    int did = sqlite3_column_int(deptStmt, 0);
                    string dname = reinterpret_cast<const char*>(sqlite3_column_text(deptStmt, 1));
                    cout << did << ". " << dname << endl;
                }
            }
            sqlite3_finalize(deptStmt);

            cout << "Department ID: ";
            int deptId;
            cin >> deptId;

            // Check if department exists
            string checkDeptSql = "SELECT COUNT(*) FROM departments WHERE id = " + to_string(deptId) + ";";
            sqlite3_stmt* checkStmt;
            bool validDept = false;
            if (sqlite3_prepare_v2(db, checkDeptSql.c_str(), -1, &checkStmt, 0) == SQLITE_OK && sqlite3_step(checkStmt) == SQLITE_ROW) {
                validDept = (sqlite3_column_int(checkStmt, 0) > 0);
            }
            sqlite3_finalize(checkStmt);

            if (!validDept) {
                cout << "Invalid department ID!" << endl;
                continue;
            }

            string sql = "INSERT INTO users (username, password, name, email, role, department_id) "
                "VALUES ('" + username + "', '" + password + "', '" + name + "', '"
                + email + "', 'student', " + to_string(deptId) + ");";
            if (executeSQL(sql.c_str())) {
                // Get the new user ID
                string idSql = "SELECT last_insert_rowid();";
                sqlite3_stmt* stmt;
                if (sqlite3_prepare_v2(db, idSql.c_str(), -1, &stmt, 0) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
                    int userId = sqlite3_column_int(stmt, 0);
                    string studentSql = "INSERT INTO students (user_id, department_id, fees_due, fees_paid) "
                        "VALUES (" + to_string(userId) + ", " + to_string(deptId) + ", 5000.0, 0.0);";
                    executeSQL(studentSql.c_str());
                }
                sqlite3_finalize(stmt);
                cout << "Student created successfully!" << endl;
            }
        }
        else if (choice == 2) {
            string sql = "INSERT INTO users (username, password, name, email, role) "
                "VALUES ('" + username + "', '" + password + "', '" + name + "', '"
                + email + "', 'professor');";
            if (executeSQL(sql.c_str())) {
                cout << "Professor created successfully!" << endl;
            }
        }
    }
}

void Admin::listUsers() {
    int choice;
    cout << "\n=== List Users ===\n";
    cout << "1. All Users\n";
    cout << "2. Students Only\n";
    cout << "3. Professors Only\n";
    cout << "Enter choice: ";
    cin >> choice;

    string sql;
    if (choice == 1) {
        sql = "SELECT id, username, name, role FROM users;";
    }
    else if (choice == 2) {
        sql = "SELECT users.id, users.username, users.name, users.role "
            "FROM users JOIN students ON users.id = students.user_id;";
    }
    else if (choice == 3) {
        sql = "SELECT id, username, name, role FROM users WHERE role = 'professor';";
    }
    else {
        cout << "Invalid choice!" << endl;
        return;
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << "\nUser List:\n";
    cout << left << setw(5) << "ID" << setw(15) << "Username" << setw(25) << "Name" << setw(10) << "Role" << endl;
    cout << string(60, '-') << endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        string username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        string role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        cout << left << setw(5) << id << setw(15) << username << setw(25) << name << setw(10) << role << endl;
    }
    sqlite3_finalize(stmt);
}

void Admin::addDepartment() {
    string name;
    cout << "\n=== Add Department ===\n";
    cout << "Department Name: ";
    cin.ignore();
    getline(cin, name);

    string sql = "INSERT INTO departments (name) VALUES ('" + name + "');";
    if (executeSQL(sql.c_str())) {
        cout << "Department added successfully!" << endl;
    }
}

void Admin::showGrades() {
    cout << "\n=== All Grades ===\n";
    string sql = "SELECT users.name, courses.name, grades.assignment1, grades.assignment2, "
        "grades.coursework, grades.final_exam, grades.total, grades.grade_letter "
        "FROM grades "
        "JOIN users ON grades.student_id = users.id "
        "JOIN courses ON grades.course_id = courses.id;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << left << setw(15) << "Student" << setw(15) << "Course" << setw(10) << "Ass1" << setw(10) << "Ass2"
        << setw(10) << "CW" << setw(10) << "Final" << setw(10) << "Total" << setw(10) << "Grade" << endl;
    cout << string(95, '-') << endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string student = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        string course = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        double ass1 = sqlite3_column_double(stmt, 2);
        double ass2 = sqlite3_column_double(stmt, 3);
        double cw = sqlite3_column_double(stmt, 4);
        double final = sqlite3_column_double(stmt, 5);
        double total = sqlite3_column_double(stmt, 6);
        string grade = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

        cout << left << setw(15) << student
            << setw(15) << course
            << setw(10) << ass1
            << setw(10) << ass2
            << setw(10) << cw
            << setw(10) << final
            << setw(10) << total
            << setw(10) << grade << endl;
    }
    sqlite3_finalize(stmt);
}

void Admin::showAttendance() {
    cout << "\n=== All Attendance ===\n";
    string sql = "SELECT users.name, courses.name, attendance.date, attendance.status "
        "FROM attendance "
        "JOIN users ON attendance.student_id = users.id "
        "JOIN courses ON attendance.course_id = courses.id;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << left << setw(20) << "Student" << setw(20) << "Course" << setw(15) << "Date" << setw(10) << "Status" << endl;
    cout << string(70, '-') << endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string student = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        string course = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        string date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        cout << left << setw(20) << student << setw(20) << course << setw(15) << date << setw(10) << status << endl;
    }
    sqlite3_finalize(stmt);
}

void Admin::addCourse() {
    cout << "\n=== Add Course ===\n";

    // List departments
    string deptSql = "SELECT id, name FROM departments;";
    sqlite3_stmt* deptStmt;
    vector<pair<int, string>> departments;

    if (sqlite3_prepare_v2(db, deptSql.c_str(), -1, &deptStmt, 0) == SQLITE_OK) {
        while (sqlite3_step(deptStmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(deptStmt, 0);
            string name = reinterpret_cast<const char*>(sqlite3_column_text(deptStmt, 1));
            departments.push_back({ id, name });
            cout << id << ". " << name << endl;
        }
    }
    sqlite3_finalize(deptStmt);

    if (departments.empty()) {
        cout << "No departments found! Add departments first." << endl;
        return;
    }

    int deptId;
    cout << "Select Department ID: ";
    cin >> deptId;

    string name, courseType;
    cout << "Course Name: ";
    cin.ignore();
    getline(cin, name);

    cout << "Course Type (theoretical/practical): ";
    cin >> courseType;
    if (courseType != "theoretical" && courseType != "practical") {
        cout << "Invalid course type! Must be 'theoretical' or 'practical'" << endl;
        return;
    }

    string sql = "INSERT INTO courses (name, department_id, course_type) "
        "VALUES ('" + name + "', " + to_string(deptId) + ", '" + courseType + "');";
    if (executeSQL(sql.c_str())) {
        cout << "Course added successfully!" << endl;
    }
}

void Admin::assignProfessor() {
    cout << "\n=== Assign Professor ===\n";

    // List professors
    string profSql = "SELECT id, name FROM users WHERE role = 'professor';";
    sqlite3_stmt* profStmt;
    vector<pair<int, string>> professors;

    if (sqlite3_prepare_v2(db, profSql.c_str(), -1, &profStmt, 0) == SQLITE_OK) {
        while (sqlite3_step(profStmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(profStmt, 0);
            string name = reinterpret_cast<const char*>(sqlite3_column_text(profStmt, 1));
            professors.push_back({ id, name });
            cout << id << ". " << name << endl;
        }
    }
    sqlite3_finalize(profStmt);

    if (professors.empty()) {
        cout << "No professors found!" << endl;
        return;
    }

    int profId;
    cout << "Select Professor ID: ";
    cin >> profId;

    // List departments
    string deptSql = "SELECT id, name FROM departments;";
    sqlite3_stmt* deptStmt;
    vector<pair<int, string>> departments;

    if (sqlite3_prepare_v2(db, deptSql.c_str(), -1, &deptStmt, 0) == SQLITE_OK) {
        while (sqlite3_step(deptStmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(deptStmt, 0);
            string name = reinterpret_cast<const char*>(sqlite3_column_text(deptStmt, 1));
            departments.push_back({ id, name });
            cout << id << ". " << name << endl;
        }
    }
    sqlite3_finalize(deptStmt);

    if (departments.empty()) {
        cout << "No departments found!" << endl;
        return;
    }

    int deptId;
    cout << "Select Department ID: ";
    cin >> deptId;

    // Assign to department
    string deptAssignSql = "INSERT OR IGNORE INTO professor_departments (professor_id, department_id) "
        "VALUES (" + to_string(profId) + ", " + to_string(deptId) + ");";
    executeSQL(deptAssignSql.c_str());

    // List courses in department
    string courseSql = "SELECT id, name FROM courses WHERE department_id = " + to_string(deptId) + ";";
    sqlite3_stmt* courseStmt;
    vector<pair<int, string>> courses;

    if (sqlite3_prepare_v2(db, courseSql.c_str(), -1, &courseStmt, 0) == SQLITE_OK) {
        while (sqlite3_step(courseStmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(courseStmt, 0);
            string name = reinterpret_cast<const char*>(sqlite3_column_text(courseStmt, 1));
            courses.push_back({ id, name });
            cout << id << ". " << name << endl;
        }
    }
    sqlite3_finalize(courseStmt);

    if (courses.empty()) {
        cout << "No courses found in this department!" << endl;
        return;
    }

    int courseId;
    cout << "Select Course ID: ";
    cin >> courseId;

    // Assign to course
    string courseAssignSql = "INSERT OR IGNORE INTO professor_courses (professor_id, course_id) "
        "VALUES (" + to_string(profId) + ", " + to_string(courseId) + ");";
    if (executeSQL(courseAssignSql.c_str())) {
        cout << "Professor assigned successfully!" << endl;
    }
}

void Admin::manageFees() {
    cout << "\n=== Manage Student Fees ===\n";

    // List students
    string sql = "SELECT users.id, users.name, students.fees_due, students.fees_paid "
        "FROM users JOIN students ON users.id = students.user_id;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) {
        cerr << "Error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    vector<pair<int, pair<double, double>>> students; // id -> (due, paid)
    cout << left << setw(5) << "ID" << setw(25) << "Name" << setw(12) << "Due" << setw(12) << "Paid" << endl;
    cout << string(55, '-') << endl;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        double due = sqlite3_column_double(stmt, 2);
        double paid = sqlite3_column_double(stmt, 3);
        students.push_back({ id, {due, paid} });
        cout << left << setw(5) << id << setw(25) << name
            << setw(12) << fixed << setprecision(2) << due
            << setw(12) << paid << endl;
    }
    sqlite3_finalize(stmt);

    if (students.empty()) {
        cout << "No students found!" << endl;
        return;
    }

    int studentId;
    double amount;
    cout << "Enter Student ID to update fees: ";
    cin >> studentId;
    cout << "Enter payment amount: ";
    cin >> amount;

    auto it = find_if(students.begin(), students.end(),
        [studentId](const pair<int, pair<double, double>>& s) { return s.first == studentId; });

    if (it == students.end()) {
        cout << "Invalid student ID!" << endl;
        return;
    }

    double newPaid = it->second.second + amount;
    double due = it->second.first;

    if (newPaid > due) {
        cout << "Payment exceeds due amount! Maximum payable: $" << (due - it->second.second) << endl;
        return;
    }

    string updateSql = "UPDATE students SET fees_paid = " + to_string(newPaid) +
        " WHERE user_id = " + to_string(studentId) + ";";
    if (executeSQL(updateSql.c_str())) {
        cout << "Fees updated successfully!" << endl;
    }
}

void Admin::displayMenu() {
    int choice;
    while (true) {
        cout << "\n=== Admin Menu ===\n";
        cout << "1. Manage Users\n";
        cout << "2. List Users\n";
        cout << "3. Add Department\n";
        cout << "4. Show All Grades\n";
        cout << "5. Show All Attendance\n";
        cout << "6. Add Course\n";
        cout << "7. Assign Professor\n";
        cout << "8. Manage Student Fees\n";
        cout << "9. Logout\n";
        cout << "Enter choice: ";
        cin >> choice;

        switch (choice) {
        case 1: manageUsers(); break;
        case 2: listUsers(); break;
        case 3: addDepartment(); break;
        case 4: showGrades(); break;
        case 5: showAttendance(); break;
        case 6: addCourse(); break;
        case 7: assignProfessor(); break;
        case 8: manageFees(); break;
        case 9: return;
        default: cout << "Invalid choice!" << endl;
        }
    }
}

// Initialize database schema
void initializeDatabase() {
    // Enable foreign keys
    executeSQL("PRAGMA foreign_keys = ON;");

    // Create tables
    executeSQL("CREATE TABLE IF NOT EXISTS departments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE);");

    executeSQL("CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL UNIQUE,"
        "password TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "email TEXT NOT NULL,"
        "role TEXT NOT NULL CHECK(role IN ('admin', 'professor', 'student')),"
        "department_id INTEGER REFERENCES departments(id));");

    executeSQL("CREATE TABLE IF NOT EXISTS students ("
        "user_id INTEGER PRIMARY KEY REFERENCES users(id),"
        "department_id INTEGER REFERENCES departments(id),"
        "fees_due REAL DEFAULT 5000.0,"
        "fees_paid REAL DEFAULT 0.0);");

    executeSQL("CREATE TABLE IF NOT EXISTS professor_departments ("
        "professor_id INTEGER REFERENCES users(id),"
        "department_id INTEGER REFERENCES departments(id),"
        "PRIMARY KEY (professor_id, department_id));");

    executeSQL("CREATE TABLE IF NOT EXISTS courses ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "department_id INTEGER REFERENCES departments(id),"
        "course_type TEXT NOT NULL CHECK(course_type IN ('theoretical', 'practical')));");

    executeSQL("CREATE TABLE IF NOT EXISTS professor_courses ("
        "professor_id INTEGER REFERENCES users(id),"
        "course_id INTEGER REFERENCES courses(id),"
        "PRIMARY KEY (professor_id, course_id));");

    executeSQL("CREATE TABLE IF NOT EXISTS grades ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "student_id INTEGER REFERENCES users(id),"
        "course_id INTEGER REFERENCES courses(id),"
        "assignment1 REAL DEFAULT 0,"
        "assignment2 REAL DEFAULT 0,"
        "coursework REAL DEFAULT 0,"
        "final_exam REAL DEFAULT 0,"
        "total REAL DEFAULT 0,"
        "grade_letter TEXT,"
        "UNIQUE(student_id, course_id));");

    executeSQL("CREATE TABLE IF NOT EXISTS attendance ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "student_id INTEGER REFERENCES users(id),"
        "course_id INTEGER REFERENCES courses(id),"
        "date TEXT NOT NULL,"
        "status TEXT NOT NULL CHECK(status IN ('present', 'absent')));");

    // Create default admin if not exists
    executeSQL("INSERT OR IGNORE INTO users (username, password, name, email, role) "
        "VALUES ('admin', 'admin123', 'System Admin', 'admin@university.com', 'admin');");
}

int main() {
    // Open database connection
    if (sqlite3_open("university.db", &db)) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

    // Initialize database schema
    initializeDatabase();

    cout << "University Management System\n";
    cout << "---------------------------\n";

    while (true) {
        User* currentUser = login();
        if (!currentUser) continue;

        currentUser->displayMenu();
        delete currentUser;
    }

    sqlite3_close(db);
    return 0;
}