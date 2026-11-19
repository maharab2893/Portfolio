# Technical Documentation: Restaurant Management System

This document provides a comprehensive technical breakdown of the Restaurant Management System architecture, data flow, module breakdown, and database design.

---

## 1. System Architecture

The application is structured as a **Single Page Application (SPA)** using pure Vanilla JavaScript, modern DOM manipulation techniques, and modular JavaScript file structures.

```
+-------------------------------------------------------------------+
|                           index.html                              |
|   +-----------------------------------------------------------+   |
|   |                       css/style.css                       |   |
|   +-----------------------------------------------------------+   |
|   |                     js/core.js (Router)                   |   |
|   +-------------------+-------------------+-------------------+   |
|                       |                   |                       |
|          +------------+           +-------+-------+               |
|          v                        v               v               |
|    js/views/*.js           js/config.js    Supabase JS SDK        |
|  (UI View Handlers)       (Env & Config)          |               |
+----------+----------------------------------------+---------------+
           |                                        |
           +-------------------> <------------------+
                                 |
                                 v
                       Supabase Cloud Backend
                        (PostgreSQL Engine)
```

---

## 2. Core Modules Explanation

### 2.1 Core Infrastructure (`js/core.js` & `js/main.js`)
- **Router / Navigation**: Listens to URL hash changes or sidebar button clicks to dynamically render views inside a main container without page reloads.
- **State Management**: Keeps local state for active order items (POS cart), user session state, and global settings.
- **Database Client Wrappers**: Encapsulates common Supabase SQL operations (GET, POST, UPDATE, DELETE).

### 2.2 Views / Controllers (`js/views/`)
1. **`dashboard.js`**: Calculates metrics such as total revenue today, total pending orders, active kitchen items, and low stock warnings.
2. **`pos.js`**:
   - Maintains real-time cart state.
   - Handles item selection, quantity adjustment, tax/discount calculation, and order submission.
3. **`kitchen.js`**:
   - Displays orders grouped by status (`Pending`, `Preparing`, `Ready`).
   - Leverages Supabase real-time subscriptions to auto-refresh when new orders arrive.
4. **`orders.js`**: Displays master order logs, filtering by status/date, and handles invoice printing.
5. **`menu.js`**: Performs CRUD (Create, Read, Update, Delete) operations on restaurant menu items and categories.
6. **`attendance.js`**: Records employee clock-in/clock-out events with timestamp tracking.
7. **`reports.js`**: Aggregates order data over specific time ranges (Daily, Weekly, Monthly) and exports or visualizes summary metrics.
8. **`settings.js`**: Manages operational parameters like currency symbols, tax rates, restaurant name, and receipt header/footer content.

---

## 3. Database Schema Overview (`supabase/schema.sql`)

The database runs on **PostgreSQL** hosted via **Supabase**. The core schema includes the following primary entities:

| Table Name | Description | Key Fields |
| :--- | :--- | :--- |
| `categories` | Food menu categories | `id`, `name`, `created_at` |
| `menu_items` | Individual dishes/drinks | `id`, `category_id`, `name`, `price`, `available` |
| `orders` | Customer order records | `id`, `order_number`, `total_amount`, `status`, `payment_type`, `created_at` |
| `order_items` | Detailed lines in an order | `id`, `order_id`, `menu_item_id`, `quantity`, `unit_price`, `subtotal` |
| `attendance` | Staff time tracking | `id`, `employee_name`, `clock_in`, `clock_out`, `date` |
| `settings` | System metadata | `key`, `value` |

---

## 4. API & Data Flow

1. **POS Order Creation Flow**:
   - User selects items -> State in `pos.js` updates.
   - User clicks "Checkout".
   - `pos.js` calls `core.js` API wrapper.
   - Insert payload into `orders` table.
   - Insert line items into `order_items` table.
   - Supabase fires real-time notification to `kitchen.js`.

2. **Kitchen KDS Realtime Flow**:
   - `kitchen.js` subscribes to changes on the `orders` table.
   - When a row status updates (e.g., `Pending` -> `Preparing`), the view re-renders automatically without manual page refresh.