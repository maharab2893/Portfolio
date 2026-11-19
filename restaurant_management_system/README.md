# Restaurant Management System 🍽️

A modern, lightweight, and modular Web-based Restaurant Management System designed for managing restaurant sales, inventory, kitchen status, staff attendance, and analytics. Powered by Vanilla JavaScript and Supabase.

---

## 🚀 Features

- **Dashboard**: High-level overview of daily sales, active orders, and restaurant metrics.
- **POS (Point of Sale)**: Quick and intuitive interface for taking orders, managing carts, and generating receipts.
- **Kitchen Display System (KDS)**: Real-time status update for kitchen staff to track and prepare incoming orders.
- **Menu Management**: Dynamic control over food items, pricing, categories, and availability.
- **Order Tracking**: Complete lifecycle management of pending, preparing, served, and completed orders.
- **Attendance System**: Track employee clock-in/clock-out times and daily staff presence.
- **Reports & Analytics**: Comprehensive financial and operational reports.
- **Settings**: System configurations, tax setup, restaurant metadata, and user roles.

---

## 🛠️ Tech Stack

- **Frontend**: HTML5, CSS3, Vanilla JavaScript (ES6+ Single Page Application Architecture)
- **Backend / Database**: [Supabase](https://supabase.com/) (PostgreSQL & Realtime engine)
- **Styling**: Custom Responsive CSS (`css/style.css`)

---

## 📂 Project Structure

```text
resturant-main/
├── index.html            # Main SPA Entry point
├── css/
│   └── style.css         # Global Stylesheet
├── js/
│   ├── config.js         # Supabase connection & global constants
│   ├── core.js           # Core routing, state management, and API helpers
│   ├── main.js           # App initialization and event listeners
│   └── views/            # UI Module Controllers
│       ├── attendance.js # Attendance view logic
│       ├── dashboard.js  # Dashboard analytics logic
│       ├── kitchen.js    # Kitchen Display System logic
│       ├── menu.js       # Menu CRUD logic
│       ├── orders.js     # Order management logic
│       ├── pos.js        # Point of Sale logic
│       ├── reports.js    # Sales reports logic
│       └── settings.js   # System configuration logic
└── supabase/
    └── schema.sql        # Database tables, triggers, and policies
```

---

## ⚙️ Installation & Setup

### Prerequisites
- A modern web browser (Chrome, Firefox, Edge, Safari).
- A [Supabase](https://supabase.com/) account for database hosting.
- A local web server (e.g., Live Server extension in VS Code).

### Step-by-Step Instructions

1. **Clone or Extract the Repository**:
   ```bash
   git clone <repository-url>
   cd resturant-main
   ```

2. **Database Setup (Supabase)**:
   - Create a new project in your Supabase Dashboard.
   - Go to the **SQL Editor** in Supabase.
   - Copy the contents of `supabase/schema.sql` and execute it to create all tables and relations.

3. **Configure API Keys**:
   - Open `js/config.js`.
   - Insert your Supabase Project URL and Anon API Key:
     ```javascript
     const SUPABASE_URL = "https://your-project.supabase.co";
     const SUPABASE_ANON_KEY = "your-anon-key";
     ```

4. **Run the Application**:
   - Serve `index.html` using a local web server (e.g., Live Server in VS Code).
   - Open your browser at `http://127.0.0.1:5500`.

---

## 📄 License
This project is open-source and available under the [MIT License](LICENSE).