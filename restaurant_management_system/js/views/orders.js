/* Orders — searchable list, receipts, collect payment, cancel */
(() => {
  'use strict';
  const RD = window.RD;
  const { $, esc } = RD;

  const O = { rows: [], wired: false, timer: null, seq: 0 };

  function wire() {
    if (O.wired) return;
    O.wired = true;
    ['#orders-status', '#orders-payment', '#orders-period'].forEach((s) =>
      $(s).addEventListener('change', () => load().catch(RD.fail)));
    $('#orders-search').addEventListener('input', () => {
      clearTimeout(O.timer);
      O.timer = setTimeout(() => load().catch(RD.fail), 250);
    });
    $('#orders-body').addEventListener('click', (e) => {
      const b = e.target.closest('button[data-act]');
      if (!b) return;
      const id = b.dataset.id;
      if (b.dataset.act === 'receipt') RD.openReceiptById(id);
      if (b.dataset.act === 'pay') editPayment(id);
      if (b.dataset.act === 'cancel') cancelOrder(id);
    });
  }

  async function load() {
    const mine = ++O.seq;
    let q = RD.sb.from('orders')
      .select('id,tracking_no,order_type,table_name,customer_name,status,payment_status,payment_method,subtotal,discount,total,created_at')
      .order('created_at', { ascending: false }).limit(200);

    const status = $('#orders-status').value;
    const pay = $('#orders-payment').value;
    const period = $('#orders-period').value;
    const text = $('#orders-search').value.trim();

    if (status) q = q.eq('status', status);
    if (pay) q = q.eq('payment_status', pay);
    if (period !== 'all') {
      const days = period === 'today' ? 0 : Number(period) - 1;
      q = q.gte('created_at', RD.startOfDay(RD.addDays(new Date(), -days)).toISOString());
    }
    if (text) q = q.ilike('tracking_no', `%${RD.escLike(text)}%`);

    const { data, error } = await q;
    if (mine !== O.seq) return;
    if (error) throw error;
    O.rows = data;
    render();
  }

  function render() {
    const totalDiscount = O.rows
      .filter((o) => o.status !== 'cancelled')
      .reduce((s, o) => s + Number(o.discount || 0), 0);
    $('#orders-count').textContent =
      `${O.rows.length} order${O.rows.length === 1 ? '' : 's'}` +
      `${O.rows.length === 200 ? ' (showing latest 200)' : ''}` +
      `${totalDiscount > 0 ? ` · ${RD.money(totalDiscount)} total discount` : ''}`;

    $('#orders-body').innerHTML = O.rows.length
      ? O.rows.map((o) => {
          const cancellable = ['incoming', 'preparing', 'ready'].includes(o.status);
          const needsPay = o.payment_status === 'unpaid' && o.status !== 'cancelled';
          // A paid order can still be corrected until the kitchen completes it
          const canEditPaid = o.payment_status === 'paid' && ['incoming', 'preparing', 'ready'].includes(o.status);
          const disc = Number(o.discount || 0);
          return `
          <tr class="hover:bg-slate-50/60">
            <td class="td font-semibold text-slate-900">${esc(o.tracking_no)}</td>
            <td class="td">${o.order_type === 'dine_in' ? esc(o.table_name || 'Dine-in') : 'Walk-in'}</td>
            <td class="td text-slate-500">${esc(o.customer_name || '—')}</td>
            <td class="td">${disc > 0
              ? `<span class="font-semibold text-rose-600">-${RD.money(disc)}</span><div class="text-xs text-slate-400">of ${RD.money(o.subtotal)}</div>`
              : '<span class="text-slate-300">—</span>'}</td>
            <td class="td font-semibold">${RD.money(o.total)}</td>
            <td class="td">${RD.pill(o.status)}</td>
            <td class="td">${RD.pill(o.payment_status)}${o.payment_method && o.payment_status === 'paid' ? `<span class="ml-2 text-xs text-slate-400 capitalize">${esc(o.payment_method)}</span>` : ''}</td>
            <td class="td text-slate-500 whitespace-nowrap">${RD.fmtDateTime(o.created_at)}</td>
            <td class="td text-right whitespace-nowrap">
              <button data-act="receipt" data-id="${o.id}" class="btn btn-ghost btn-sm">Receipt</button>
              ${needsPay ? `<button data-act="pay" data-id="${o.id}" class="btn btn-soft btn-sm ml-1">Collect payment</button>` : ''}
              ${canEditPaid ? `<button data-act="pay" data-id="${o.id}" class="btn btn-ghost btn-sm ml-1">Edit payment</button>` : ''}
              ${cancellable ? `<button data-act="cancel" data-id="${o.id}" class="btn btn-ghost btn-sm ml-1 !text-rose-600">Cancel</button>` : ''}
            </td>
          </tr>`;
        }).join('')
      : '<tr><td colspan="9" class="td text-center text-slate-400 py-12">No orders match these filters.</td></tr>';
  }

  /* Mark an order Paid or Unpaid and pick the method.
     Unpaid is only offered while the order is not completed, so a finished order can't be un-paid by accident. */
  function editPayment(id) {
    const o = O.rows.find((r) => r.id === id);
    if (!o) return;
    const canUnpay = o.status !== 'completed';
    let status = o.payment_status;                       // 'paid' | 'unpaid'
    let method = o.payment_method || 'cash';
    if (status === 'unpaid') status = 'paid';            // "Collect payment" starts on Paid

    const m = RD.openModal({
      title: `${o.payment_status === 'paid' ? 'Edit payment' : 'Collect payment'} · ${o.tracking_no}`, size: 'max-w-sm',
      html: `
        <p class="text-sm text-slate-500">Amount</p>
        <p class="text-3xl font-bold text-slate-900 mt-1">${RD.money(o.total)}</p>
        ${Number(o.discount) > 0 ? `<p class="text-sm text-rose-600 mt-1">Includes a discount of ${RD.money(o.discount)}</p>` : ''}
        ${canUnpay ? `
        <label class="label mt-5">Payment status</label>
        <div id="pay-status" class="seg seg-block">
          <button data-status="paid">Paid</button>
          <button data-status="unpaid">Unpaid</button>
        </div>` : ''}
        <div id="pay-method-wrap" class="mt-4">
          <label class="label">Payment method</label>
          <div id="pay-method" class="seg seg-block">
            <button data-method="cash">Cash</button>
            <button data-method="card">Card</button>
            <button data-method="mobile">Mobile</button>
          </div>
        </div>
        <p class="text-xs text-slate-400 mt-4">${canUnpay ? 'You can change this until the order is completed.' : 'This order is completed, so it can only be marked as paid.'}</p>
        <div class="mt-6 flex justify-end gap-2">
          <button data-close class="btn btn-ghost">Cancel</button>
          <button id="pay-confirm" class="btn btn-primary">Save</button>
        </div>`,
    });

    const paint = () => {
      m.$$('#pay-status button').forEach((x) => x.classList.toggle('active', x.dataset.status === status));
      m.$$('#pay-method button').forEach((x) => x.classList.toggle('active', x.dataset.method === method));
      m.$('#pay-method-wrap').classList.toggle('hidden', status !== 'paid');
      m.$('#pay-confirm').textContent = status === 'paid' ? 'Save as paid' : 'Save as unpaid';
    };
    paint();

    const statusBox = m.$('#pay-status');
    if (statusBox) statusBox.addEventListener('click', (e) => {
      const b = e.target.closest('button[data-status]');
      if (b) { status = b.dataset.status; paint(); }
    });
    m.$('#pay-method').addEventListener('click', (e) => {
      const b = e.target.closest('button[data-method]');
      if (b) { method = b.dataset.method; paint(); }
    });

    m.$('#pay-confirm').addEventListener('click', (e) => RD.busy(e.currentTarget, async () => {
      if (status === 'unpaid' && !canUnpay) return;      // safety net: completed orders stay paid
      const patch = status === 'paid'
        ? { payment_status: 'paid', payment_method: method }
        : { payment_status: 'unpaid', payment_method: null, paid_at: null };
      const { error } = await RD.sb.from('orders').update(patch).eq('id', id);
      if (error) throw error;
      m.close();
      RD.toast(status === 'paid' ? `${o.tracking_no} marked as paid` : `${o.tracking_no} marked as unpaid`);
      await load();
    }));
  }

  async function cancelOrder(id) {
    const o = O.rows.find((r) => r.id === id);
    if (!o) return;
    const msg = o.payment_status === 'paid'
      ? `Cancel ${o.tracking_no}? It is already paid, so remember to refund the customer. Stock will be returned.`
      : `Cancel ${o.tracking_no}? Stock will be returned to inventory.`;
    if (!(await RD.confirm(msg, { title: 'Cancel this order?', confirmLabel: 'Cancel order' }))) return;
    try {
      const { error } = await RD.sb.from('orders').update({ status: 'cancelled' }).eq('id', id);
      if (error) throw error;
      RD.toast(`${o.tracking_no} cancelled`);
      await load();
      await RD.loadInventory();
    } catch (e) { RD.fail(e); }
  }

  RD.views.orders = {
    async mount() { wire(); await load(); },
    async refresh() { await load(); },
  };
})();
