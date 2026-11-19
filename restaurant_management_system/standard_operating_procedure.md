# Standard Operating Procedure (SOP): Restaurant Management System

This document provides operational guidelines for deploying, maintaining, troubleshooting, and operating the Restaurant Management System.

---

## 1. Environment Setup & Deployment SOP

### 1.1 Supabase Setup
1. Log in to [Supabase Console](https://app.supabase.com/).
2. Click **New Project** and name it (e.g., `Restaurant-Production`).
3. Set a strong Database Password and select your nearest region.
4. Navigate to **SQL Editor** -> **New Query**.
5. Copy the full contents from `supabase/schema.sql` in this repo, paste it into the editor, and click **Run**.
6. Navigate to **Project Settings** -> **API** to retrieve:
   - Project URL
   - `anon` Public API Key

### 1.2 Frontend Deployment (Static Hosting)
Because this application is built with static HTML/CSS/JS, it can be hosted on **Vercel**, **Netlify**, or **GitHub Pages**.

#### Deploying via Netlify / Vercel:
1. Connect your GitHub repository.
2. Set Build Command: *(Leave blank)*
3. Set Publish Directory: `./` or `resturant-main/`
4. Deploy site.

---

## 2. Daily Operational Procedures

### 2.1 Opening Shift (Daily Setup)
1. Launch the web browser and open the Restaurant App URL.
2. Verify Internet connectivity (required for Supabase sync).
3. Navigate to **Attendance** -> Ensure staff clock in.
4. Check **Menu** view to mark any out-of-stock items as `Unavailable`.

### 2.2 POS & Order Processing Workflow
1. Navigate to the **POS** view.
2. Select target categories and add food items to the cart.
3. Apply any applicable discounts or notes.
4. Select payment method (`Cash`, `Card`, `Digital Wallet`).
5. Complete payment -> Receipt prints or saves -> Order status set to `Pending`.

### 2.3 Kitchen Display System (KDS) Workflow
1. Keep a dedicated tablet/monitor open on the **Kitchen** view.
2. As new orders appear under `Pending`, click **Start Preparing** (`Pending` -> `Preparing`).
3. When cooked/plated, click **Mark Ready** (`Preparing` -> `Ready`).
4. Waiter serves order and updates status to `Completed`.

### 2.4 Closing Shift
1. Staff navigate to **Attendance** -> Clock out.
2. Manager opens **Reports** view.
3. Review total sales, order count, and payment breakdowns for the day.
4. Export or print daily sales summary report.

---

## 3. Maintenance & Backup Procedures

### 3.1 Database Backups
- Supabase performs automatic daily database backups for paid tiers.
- For manual backup on free tier:
  1. Go to Supabase Dashboard -> **Database** -> **Backups**.
  2. Or export data tables as CSV via the **Table Editor** (`orders`, `order_items`, `attendance`).

### 3.2 Updating Code & Adding Features
1. Test all changes locally before deploying.
2. Never modify `schema.sql` live on production without testing migrations.
3. When updating JS scripts, perform hard browser refresh (`Ctrl + F5` or `Cmd + Shift + R`) on terminal devices to clear cached files.

---

## 4. Troubleshooting Guide

| Issue | Potential Cause | Solution |
| :--- | :--- | :--- |
| **Data not loading / Blank screens** | Invalid API keys in `config.js` | Verify `SUPABASE_URL` and `SUPABASE_ANON_KEY` values in `js/config.js`. |
| **Kitchen screen not updating live** | Real-time not enabled on Supabase | Go to Supabase Dashboard -> **Database** -> **Replication** and enable Realtime for `orders` table. |
| **Orders failing to submit** | Missing table policies or foreign keys | Re-run `schema.sql` script or check browser developer console (`F12`) for SQL error logs. |
| **CSS layout breaks on tablet** | Viewport caching or low resolution | Clear browser cache or ensure device resolution meets responsive breakpoints in `css/style.css`. |