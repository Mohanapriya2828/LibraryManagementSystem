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
void addBook() {
    if (currentUserRole != "Admin") {
    cout << "Access denied. Only admin can perform this action.\n";
    return;
}

    string title, genre, publisher, isbn, edition, rack, language;
    int year;
    double price;

    cout << "Title: "; getline(cin, title);
    cout << "Genre: "; getline(cin, genre);
    cout << "Publisher: "; getline(cin, publisher);
    cout << "ISBN (unique): "; getline(cin, isbn);
    if (exists("SELECT * FROM books WHERE isbn='" + isbn + "'")) {
        cout << "ISBN already exists.\n"; return;
    }
    cout << "Edition: "; getline(cin, edition);
    cout << "Year: "; cin >> year; cin.ignore();
    cout << "Price: "; cin >> price; cin.ignore();
    cout << "Rack: "; getline(cin, rack);
    cout << "Language: "; getline(cin, language);

    string sql = "INSERT INTO books (title, genre, publisher, isbn, edition, publishedyear, price, racklocation, language, availability) "
                 "VALUES ('" + title + "','" + genre + "','" + publisher + "','" + isbn + "','" + edition + "'," +
                 to_string(year) + "," + to_string(price) + ",'" + rack + "','" + language + "', 1)";
    
    cout << (execSQL(sql) ? "Book added successfully.\n" : "Failed to add book.\n");
}

void updateBook() {
    if (currentUserRole != "Admin") {
    cout << "Access denied. Only admin can perform this action.\n";
    return;
}

    string isbn;
    cout << "Enter ISBN to update: "; getline(cin, isbn);
    if (!exists("SELECT * FROM books WHERE isbn='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }

    string title, genre;
    cout << "New Title (leave blank to skip): "; getline(cin, title);
    cout << "New Genre (leave blank to skip): "; getline(cin, genre);

    string sql = "UPDATE books SET ";
    if (!title.empty()) sql += "title='" + title + "'";
    if (!genre.empty()) {
        if (!title.empty()) sql += ", ";
        sql += "genre='" + genre + "'";
    }
    sql += " WHERE isbn='" + isbn + "'";
    cout << (execSQL(sql) ? "Book updated.\n" : "Update failed.\n");
}

void deleteBook() {
    if (currentUserRole != "Admin") {
    cout << "Access denied. Only admin can perform this action.\n";
    return;
}

    string isbn;
    cout << "Enter ISBN to delete: "; getline(cin, isbn);
    if (!exists("SELECT * FROM books WHERE isbn='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }
    if (exists("SELECT * FROM transactions WHERE bookid = (SELECT bookid FROM books WHERE isbn='" + isbn + "') AND returndate IS NULL")) {
        cout << "Book currently issued, cannot delete.\n"; return;
    }
    cout << (execSQL("DELETE FROM books WHERE isbn='" + isbn + "'") ? "Book deleted.\n" : "Delete failed.\n");
}

void viewBooks() {
    int pageSize = 5, page = 0;
    string choice;
    do {
        int offset = page * pageSize;
        string sql = "SELECT bookid, title, isbn, availability FROM books ORDER BY title OFFSET " +
                     to_string(offset) + " ROWS FETCH NEXT " + to_string(pageSize) + " ROWS ONLY";

        SQLHSTMT stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
            cout << "\nPage " << page + 1 << "\nID\tTitle\t\tISBN\tAvailability\n";
            while (SQLFetch(stmt) == SQL_SUCCESS) {
                int id, avail;
                char title[255], isbn[20];
                SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
                SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
                SQLGetData(stmt, 3, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
                SQLGetData(stmt, 4, SQL_C_LONG, &avail, 0, NULL);
                cout << id << "\t" << title << "\t" << isbn << "\t" << (avail ? "Yes" : "No") << "\n";
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        cout << "[N]ext, [P]revious, [Q]uit: ";
        getline(cin, choice);
        if (choice == "N" || choice == "n") page++;
        else if ((choice == "P" || choice == "p") && page > 0) page--;
    } while (choice != "Q" && choice != "q");
}

void searchBooks() {
    cout << "Enter keyword to search in title: ";
    string keyword;
    getline(cin, keyword);
    string sql = "SELECT bookid, title, isbn FROM books WHERE title LIKE '%" + keyword + "%'";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "ID\tTitle\t\tISBN\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id;
            char title[255], isbn[20];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
            cout << id << "\t" << title << "\t" << isbn << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

void bulkImportBooks() {
    if (currentUserRole != "Admin") {
    cout << "Access denied. Only admin can perform this action.\n";
    return;
}

    string file;
    cout << "Enter CSV file path: ";
    getline(cin, file);
    ifstream in(file);
    if (!in) {
        cout << "File not found.\n";
        return;
    }

    string line;
    int success = 0, fail = 0, lineNo = 0;

    while (getline(in, line)) {
        lineNo++;
        if (line.empty()) continue;

        stringstream ss(line);
        string title, genre, publisher, isbn, edition, year, price, rack, lang;

        if (!getline(ss, title, ',') || !getline(ss, genre, ',') || !getline(ss, publisher, ',') ||
            !getline(ss, isbn, ',') || !getline(ss, edition, ',') || !getline(ss, year, ',') ||
            !getline(ss, price, ',') || !getline(ss, rack, ',') || !getline(ss, lang)) {
            cout << "Line " << lineNo << ": Incorrect format.\n";
            fail++;
            continue;
        }

        string sql = "INSERT INTO books (title, genre, publisher, isbn, edition, publishedyear, price, racklocation, language, availability) "
                     "VALUES ('" + title + "','" + genre + "','" + publisher + "','" + isbn + "','" + edition + "'," +
                     year + "," + price + ",'" + rack + "','" + lang + "', 1)";

        if (execSQL(sql))
            success++;
        else {
            cout << "Line " << lineNo << ": Failed to insert.\n";
            fail++;
        }
    }

    cout << "Bulk import complete. Success: " << success << ", Failed: " << fail << "\n";
}

void addMember() {
    if (currentUserRole != "Admin") {
    cout << "Access denied. Only admin can perform this action.\n";
    return;
}

    string name, email, phone, address, membership, status = "Active";
    cout << "Name: "; getline(cin, name);
    cout << "Email: "; getline(cin, email);
    if (exists("SELECT * FROM members WHERE email='" + email + "'")) {
        cout << "Email already exists.\n"; return;
    }
    cout << "Phone: "; getline(cin, phone);
    cout << "Address: "; getline(cin, address);
    cout << "Membership Type (Regular/Premium): "; getline(cin, membership);
    string sql = "INSERT INTO members (name, email, phone, address, membershiptype, joindate, status) "
                 "VALUES ('" + name + "','" + email + "','" + phone + "','" + address + "','" + membership + "',GETDATE(),'" + status + "')";
    cout << (execSQL(sql) ? "Member added.\n" : "Failed to add member.\n");
}

void updateMember() {
    if (currentUserRole != "Admin") {
    cout << "Access denied. Only admin can perform this action.\n";
    return;
}

    string email;
    cout << "Enter email of member to update: "; getline(cin, email);
    if (!exists("SELECT * FROM members WHERE email='" + email + "'")) {
        cout << "Member not found.\n"; return;
    }

    string phone, address;
    cout << "New phone: "; getline(cin, phone);
    cout << "New address: "; getline(cin, address);

    string sql = "UPDATE members SET phone='" + phone + "', address='" + address + "' WHERE email='" + email + "'";
    cout << (execSQL(sql) ? "Member updated.\n" : "Failed to update.\n");
}

void deleteMember() {
    if (currentUserRole != "Admin") {
    cout << "Access denied. Only admin can perform this action.\n";
    return;
}

    string email;
    cout << "Enter email of member to delete: "; getline(cin, email);
    if (!exists("SELECT * FROM members WHERE email='" + email + "'")) {
        cout << "Member not found.\n"; return;
    }
    string sql = "UPDATE members SET status='Inactive' WHERE email='" + email + "'";
    cout << (execSQL(sql) ? "Member deactivated.\n" : "Failed to deactivate.\n");
}

void viewMembers() {
    string sql = "SELECT memberid, name, email, status FROM members";
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "\nID\tName\t\tEmail\t\tStatus\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id; char name[255], email[255], status[20];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, status, sizeof(status), NULL);
            cout << id << "\t" << name << "\t" << email << "\t" << status << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

void searchMembers() {
    string keyword;
    cout << "Enter name/email to search: ";
    getline(cin, keyword);
    string sql = "SELECT memberid, name, email FROM members WHERE name LIKE '%" + keyword + "%' OR email LIKE '%" + keyword + "%'";
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "\nID\tName\t\tEmail\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id; char name[255], email[255];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
            cout << id << "\t" << name << "\t" << email << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}
void issueBook() {
    int memberId, bookId;
    cout << "Enter Member ID: ";
    cin >> memberId;
    cout << "Enter Book ID: ";
    cin >> bookId;
    cin.ignore();

    string checkBookSQL = "SELECT availability FROM books WHERE bookid = " + to_string(bookId);
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    SQLExecDirect(stmt, (SQLCHAR*)checkBookSQL.c_str(), SQL_NTS);
    
    int available = 0;
    if (SQLFetch(stmt) == SQL_SUCCESS)
        SQLGetData(stmt, 1, SQL_C_LONG, &available, 0, NULL);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (!available) {
        cout << "Book is not available.\n";
        return;
    }

    string checkLimitSQL = "SELECT COUNT(*) FROM transactions WHERE memberid = " + to_string(memberId) + " AND returndate IS NULL";
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    SQLExecDirect(stmt, (SQLCHAR*)checkLimitSQL.c_str(), SQL_NTS);

    int count = 0;
    if (SQLFetch(stmt) == SQL_SUCCESS)
        SQLGetData(stmt, 1, SQL_C_LONG, &count, 0, NULL);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (count >= 5) {
        cout << "Member has reached max limit (5 books).\n";
        return;
    }

    string sql = "INSERT INTO transactions (memberid, bookid, issuedate, duedate) VALUES (" +
                 to_string(memberId) + "," + to_string(bookId) + ", GETDATE(), DATEADD(day, 14, GETDATE()))";
    if (execSQL(sql)) {
        execSQL("UPDATE books SET availability = 0 WHERE bookid = " + to_string(bookId));
        cout << "Book issued successfully.\n";
    } else {
        cout << "Failed to issue book.\n";
    }
}

void returnBook() {
    int bookId;
    cout << "Enter Book ID to return: ";
    cin >> bookId;
    cin.ignore();

    string sql = "SELECT transactionid, DATEDIFF(day, duedate, GETDATE()) AS late_days "
                 "FROM transactions WHERE bookid = " + to_string(bookId) + " AND returndate IS NULL";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        cout << "Error retrieving transaction.\n";
        return;
    }

    int txId = 0, lateDays = 0;
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_SLONG, &txId, 0, NULL);
        SQLGetData(stmt, 2, SQL_C_SLONG, &lateDays, 0, NULL);
    } else {
        cout << "No active transaction found.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    double fine = lateDays > 0 ? lateDays * 5.0 : 0.0;
    string updateSQL = "UPDATE transactions SET returndate = GETDATE(), fineamount = " + to_string(fine) +
                       " WHERE transactionid = " + to_string(txId);
    execSQL(updateSQL);
    execSQL("UPDATE books SET availability = 1 WHERE bookid = " + to_string(bookId));
    cout << "Book returned. Fine: ₹" << fine << "\n";
}

void reserveBook() {
    int bookId, memberId;
    cout << "Enter Book ID to reserve: ";
    cin >> bookId;
    cout << "Enter Member ID: ";
    cin >> memberId;
    cin.ignore();

    string check = "SELECT * FROM books WHERE bookid = " + to_string(bookId) + " AND availability = 0";
    if (!exists(check)) {
        cout << "Book is available, no need to reserve.\n";
        return;
    }

    string sql = "INSERT INTO transactions (memberid, bookid) VALUES (" + to_string(memberId) + ", " + to_string(bookId) + ")";
    if (execSQL(sql))
        cout << "Book reserved successfully.\n";
    else
        cout << "Failed to reserve.\n";
}

void transactionHistory() {
    string filter;
    cout << "Search by:\n1. Member ID\n2. Book ID\n> ";
    getline(cin, filter);

    string input;
    cout << "Enter ID: ";
    getline(cin, input);

    string sql = string("SELECT transactionid, memberid, bookid, issuedate, duedate, returndate, fineamount ")
           + "FROM transactions WHERE " + (filter == "1" ? "memberid" : "bookid") + " = " + input;

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "\nID\tMemID\tBookID\tIssue\tDue\tReturn\tFine\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int tx, mem, bk;
            char issue[20], due[20], ret[20];
            double fine;
            SQLGetData(stmt, 1, SQL_C_SLONG, &tx, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &mem, 0, NULL);
            SQLGetData(stmt, 3, SQL_C_SLONG, &bk, 0, NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, issue, sizeof(issue), NULL);
            SQLGetData(stmt, 5, SQL_C_CHAR, due, sizeof(due), NULL);
            SQLGetData(stmt, 6, SQL_C_CHAR, ret, sizeof(ret), NULL);
            SQLGetData(stmt, 7, SQL_C_DOUBLE, &fine, 0, NULL);

            cout << tx << "\t" << mem << "\t" << bk << "\t" << issue << "\t" << due << "\t" << ret << "\t" << fine << "\n";
        }
    } else {
        cout << "No records found.\n";
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

void viewTopIssuedBooks() {
    string sql = "SELECT TOP 10 bookid, COUNT(*) AS issue_count FROM transactions GROUP BY bookid ORDER BY issue_count DESC";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "Top 10 Issued Books:\nBookID\tIssueCount\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int bookid;
            int count;
            SQLGetData(stmt, 1, SQL_C_SLONG, &bookid, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &count, 0, NULL);

            cout << bookid << "\t" << count << "\n";
        }
    } else {
        cout << "Failed to fetch top issued books.\n";
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}


void viewMostActiveMembers() {
    string sql = "SELECT TOP 10 memberid, COUNT(*) AS transaction_count FROM transactions GROUP BY memberid ORDER BY transaction_count DESC";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "Most Active Members:\nMemberID\tTransactionCount\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int memberid;
            int count;
            SQLGetData(stmt, 1, SQL_C_SLONG, &memberid, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &count, 0, NULL);

            cout << memberid << "\t" << count << "\n";
        }
    } else {
        cout << "Failed to fetch active members.\n";
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}
void fineCollectionSummary() {
    string sql = "SELECT SUM(fineamount) AS total_fines, COUNT(*) AS total_transactions_with_fine FROM transactions WHERE fineamount > 0";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        if (SQLFetch(stmt) == SQL_SUCCESS) {
            double totalFines = 0;
            int totalTransactions = 0;
            SQLGetData(stmt, 1, SQL_C_DOUBLE, &totalFines, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_SLONG, &totalTransactions, 0, NULL);

            cout << "Fine Collection Summary:\n";
            cout << "Total Fines Collected: $" << totalFines << "\n";
            cout << "Number of Transactions with Fine: " << totalTransactions << "\n";
        } else {
            cout << "No fine data found.\n";
        }
    } else {
        cout << "Failed to fetch fine collection summary.\n";
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}


#include <fstream>

void exportReportsToCSV() {
    ofstream file("LibraryReports.csv");
    if (!file.is_open()) {
        cout << "Failed to open file for writing.\n";
        return;
    }

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    file << "Top 10 Issued Books\n";
    file << "BookID,Title,IssueCount\n";

    string sql1 = "SELECT TOP 10 b.bookid, b.title, COUNT(t.bookid) AS issue_count "
                  "FROM transactions t "
                  "JOIN books b ON t.bookid = b.bookid "
                  "GROUP BY b.bookid, b.title "
                  "ORDER BY issue_count DESC";

    if (SQLExecDirect(stmt, (SQLCHAR*)sql1.c_str(), SQL_NTS) == SQL_SUCCESS) {
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int bookid, count;
            char title[255];
            SQLGetData(stmt, 1, SQL_C_SLONG, &bookid, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
            SQLGetData(stmt, 3, SQL_C_SLONG, &count, 0, NULL);
            file << bookid << "," << title << "," << count << "\n";
        }
    } else {
        file << "Failed to fetch top issued books.\n";
    }

    file << "\n";

    file << "Most Active Members\n";
    file << "MemberID,Name,TransactionCount\n";

    SQLFreeStmt(stmt, SQL_CLOSE);
    string sql2 = "SELECT TOP 10 m.memberid, m.name, COUNT(t.transactionid) AS txn_count "
                  "FROM transactions t "
                  "JOIN members m ON t.memberid = m.memberid "
                  "GROUP BY m.memberid, m.name "
                  "ORDER BY txn_count DESC";

    if (SQLExecDirect(stmt, (SQLCHAR*)sql2.c_str(), SQL_NTS) == SQL_SUCCESS) {
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int memberid, count;
            char name[255];
            SQLGetData(stmt, 1, SQL_C_SLONG, &memberid, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_SLONG, &count, 0, NULL);
            file << memberid << "," << name << "," << count << "\n";
        }
    } else {
        file << "Failed to fetch active members.\n";
    }

    file << "\n";

    file << "Fine Collection Summary\n";
    file << "TotalFineAmount\n";

    SQLFreeStmt(stmt, SQL_CLOSE);
    string sql3 = "SELECT SUM(fineamount) FROM transactions WHERE fineamount IS NOT NULL";

    if (SQLExecDirect(stmt, (SQLCHAR*)sql3.c_str(), SQL_NTS) == SQL_SUCCESS) {
        double totalFine = 0.0;
        if (SQLFetch(stmt) == SQL_SUCCESS) {
            SQLGetData(stmt, 1, SQL_C_DOUBLE, &totalFine, 0, NULL);
            file << totalFine << "\n";
        }
    } else {
        file << "Failed to fetch fine summary.\n";
    }

    file.close();
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    cout << "Report exported to LibraryReports.csv\n";
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

void memberMenu() {
    string choice;
    do {
        cout << "\n--- Members Menu ---\n";
        cout << "1. View Members\n2. Search Members\n";

        if (currentUserRole == "Admin") {
            cout << "3. Add Member\n4. Update Member\n5. Delete Member\n";
        }

        cout << "0. Back\n> ";
        getline(cin, choice);

        if (choice == "1") viewMembers();
        else if (choice == "2" ) searchMembers();
        else if (choice == "3" && currentUserRole == "Admin") addMember();
        else if (choice == "4" && currentUserRole == "Admin") updateMember();
        else if (choice == "5" && currentUserRole == "Admin") deleteMember();
        else if (choice == "0") break;
        else cout << "Invalid option.\n";
    } while (true);
}
void transactionMenu() {
    string choice;
    do {
        cout << "\n--- Transactions Menu ---\n";

        if (currentUserRole == "Admin") {
            cout << "1. Issue Book\n";
            cout << "2. Return Book\n";
            cout << "3. Reserve Book\n";
            cout << "4. Transaction History\n";
        } else {
            cout << "1. Return Book\n";
            cout << "2. Reserve Book\n";
            cout << "3. Transaction History\n";
        }

        cout << "0. Back\n> ";
        getline(cin, choice);

        if (currentUserRole == "Admin") {
            if (choice == "1") issueBook();
            else if (choice == "2") returnBook();
            else if (choice == "3") reserveBook();
            else if (choice == "4") transactionHistory();
            else if (choice == "0") break;
            else cout << "Invalid option.\n";
        } else { 
            if (choice == "1") returnBook();
            else if (choice == "2") reserveBook();
            else if (choice == "3") transactionHistory();
            else if (choice == "0") break;
            else cout << "Invalid option.\n";
        }
    } while (true);
}

void reportsMenu() {
    string choice;
    do {
        cout << "\n--- Reports Menu ---\n";
        cout << "1. View Top 10 Issued Books\n";
        cout << "2. View Most Active Members\n";
        cout << "3. Fine Collection Summary\n";
        cout << "4. Export Top 10 Issued Books to CSV\n";
        cout << "0. Back\n> ";
        getline(cin, choice);

        if (choice == "1" && currentUserRole == "Admin") viewTopIssuedBooks();
        else if (choice == "2" && currentUserRole == "Admin") viewMostActiveMembers();
        else if (choice == "3" && currentUserRole == "Admin") fineCollectionSummary();
        else if (choice == "4" && currentUserRole == "Admin") exportReportsToCSV();
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