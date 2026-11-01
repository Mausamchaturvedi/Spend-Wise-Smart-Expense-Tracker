# SpendWise – Smart Expense Tracker (C++)

A desktop-browser-based personal finance tracker with a C++ backend, HTML/CSS/JavaScript frontend, and local CSV file storage. It does not require MySQL, SQLite, or any other database.

## Features

- Dashboard showing total income, expenses, balance and current-month net balance.
- Add income and expense transactions.
- Search and filter transaction history.
- Delete transactions.
- Set monthly budgets by category.
- Compare budget limits with recorded spending.
- Monthly income-versus-expense chart.
- Persistent local storage in CSV files.
- Responsive interface.

## Technology

- Backend: C++17, standard library, native TCP sockets.
- Frontend: HTML5, CSS3, vanilla JavaScript.
- Storage: CSV files.
- IDE: Visual Studio Code.

## Requirements

- Windows 10/11, Linux, or macOS.
- A C++17 compiler.
  - Windows: MinGW-w64 g++ (MSYS2 UCRT64 recommended).
  - Linux: g++.
  - macOS: Xcode Command Line Tools / clang++.
- A modern browser.

## Run on Windows (VS Code terminal)

Open this project folder in VS Code. Open Terminal > New Terminal and run:

```powershell
g++ -std=c++17 -O2 src/server.cpp -o spendwise.exe -lws2_32
.\spendwise.exe
```

Keep the terminal open. Visit:

http://localhost:8080

To stop the server, press `Ctrl+C` in the terminal.

## Run on Linux

```bash
g++ -std=c++17 -O2 src/server.cpp -o spendwise
./spendwise
```

Then visit http://localhost:8080.

## Run on macOS

```bash
clang++ -std=c++17 -O2 src/server.cpp -o spendwise
./spendwise
```

Then visit http://localhost:8080.

## Data storage

On the first run, the backend creates:

```text
data/
  transactions.csv
  budgets.csv
```

These files are created automatically and retain information between runs. Back them up before manually editing them. To reset the application, stop the server and remove the CSV files; the server recreates empty files on its next launch.

## Project structure

```text
Smart_Expense_Tracker_CPP/
├── src/
│   └── server.cpp
├── frontend/
│   ├── index.html
│   ├── style.css
│   └── app.js
├── data/
│   └── (created automatically)
├── .vscode/
│   ├── tasks.json
│   └── launch.json
├── .gitignore
└── README.md
```

## API endpoints

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/transactions` | List transactions |
| POST | `/api/transactions` | Add transaction |
| DELETE | `/api/transactions?id=...` | Delete transaction |
| GET | `/api/summary` | Get dashboard totals |
| GET | `/api/budgets` | List budgets |
| POST | `/api/budgets` | Create/update a monthly budget |

POST requests use URL-encoded form data.

## Important notes

- This is a local college mini-project, not a production financial application.
- The built-in server is intended for use on your own computer. Do not expose it to the public internet.
- No user authentication is implemented in this version.
- The chart is drawn with the browser Canvas API; no charting library is required.
- Google Fonts are optional. If offline, the browser uses fallback fonts.
- The backend handles one client request at a time, which is sufficient for a local demonstration.
