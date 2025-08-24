#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
using namespace std;


SQLHENV env = NULL;
SQLHDBC dbc = NULL;
string currentUserRole;


void showError(const char* fn, SQLHANDLE handle, SQLSMALLINT type) {
    SQLINTEGER i = 0, native;
    SQLCHAR state[7], text[256];
    SQLSMALLINT len;
    while (SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len) == SQL_SUCCESS)
        cerr << fn << " error: " << state << " " << native << " " << text << "\n";
}


bool exists(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }
    SQLRETURN r = SQLFetch(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
}


bool execSQL(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) showError("execSQL", stmt, SQL_HANDLE_STMT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(ret);
}

bool login() {
    string username, password;
    cout << "\n=== Login ===\nUsername: ";
    getline(cin, username);
    cout << "Password: ";
    getline(cin, password);  

    string sql = "SELECT role, status FROM users WHERE username='" + username +
                 "' AND passwordhash='" + password + "'";
    
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        showError("Login", stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }

    SQLCHAR role[20], status[20];
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_CHAR, role, sizeof(role), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, status, sizeof(status), NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        if (string((char*)status) == "Inactive") {
            cout << "Account is inactive.\n";
            return false;
        }

        currentUserRole = (char*)role;
        cout << "Login successful. Role: " << currentUserRole << "\n";
        return true;
    } else {
        cout << "Invalid credentials.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
}

void bookMenu() {
    string choice;
    do {
        cout << "\n--- Books Menu ---\n";
        cout << "1. View\n2. Search\n3. Bulk Import\n";

        if (currentUserRole == "Admin") {
            cout << "4. Add Book\n5. Update Book\n6. Delete Book\n";
        }

        cout << "0. Back\n> ";
        getline(cin, choice);

        if (choice == "1") viewBooks();
        else if (choice == "2") searchBooks();
        else if (choice == "3" && currentUserRole == "Admin") bulkImportBooks();
        else if (choice == "4" && currentUserRole == "Admin") addBook();
        else if (choice == "5" && currentUserRole == "Admin") updateBook();
        else if (choice == "6" && currentUserRole == "Admin") deleteBook();
        else if (choice == "0") break;
        else cout << "Invalid option.\n";
    } while (true);
}
void mainMenu() {
    string choice;
    do {
        cout << "\n========== Library Management ==========\n"
             << "1. Books Management\n"
             << "2. Members Management\n"
             << "3. Transactions\n"
             << "4. Reports and analytics\n"
             << "5. Exit\n> ";
        getline(cin, choice);
        if (choice == "1") bookMenu();
        else if (choice == "2") memberMenu();
        else if (choice == "3") transactionMenu();
        else if (choice == "4") reportsMenu();
        else if (choice == "5") break;
        else cout << "Invalid option.\n";
    } while (true);
}



int main() {
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    SQLCHAR connStr[] = "DRIVER={ODBC Driver 17 for SQL Server};SERVER=PSILENL057;DATABASE=Library_management;Trusted_Connection=Yes;";
    if (!SQL_SUCCEEDED(SQLDriverConnect(dbc, NULL, connStr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE))) {
        showError("Connection", dbc, SQL_HANDLE_DBC);
        return 1;
    }

    if (!login()) {
        SQLDisconnect(dbc); SQLFreeHandle(SQL_HANDLE_DBC, dbc); SQLFreeHandle(SQL_HANDLE_ENV, env);
        return 0;
    }

    mainMenu();

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return 0;
}