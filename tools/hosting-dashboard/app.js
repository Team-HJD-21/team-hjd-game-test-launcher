(() => {
  'use strict';
  // Fixed, hash-authorized script. No external code, telemetry or persistent email storage.
  if (location.protocol !== 'http:' || location.hostname !== '127.0.0.1') return;
  document.documentElement.classList.add('enhanced');
  const $ = id => document.getElementById(id);
  let page = 1, paused = false, busy = false, pendingForm = null;
  const pageSize = 25;
  function view(name, focus = false) {
    if (!['overview', 'testers', 'games', 'hosting'].includes(name)) name = 'overview';
    document.querySelectorAll('[data-view]').forEach(el => { el.hidden = el.dataset.view !== name; });
    document.querySelectorAll('[data-nav]').forEach(el => {
      if (el.dataset.nav === name) el.setAttribute('aria-current', 'page'); else el.removeAttribute('aria-current');
    });
    if (focus) document.querySelector(`[data-nav="${name}"]`).focus();
  }
  function filter() {
    const query = $('tester-search').value.trim().toLocaleLowerCase();
    const period = $('tester-period').value;
    const order = $('tester-sort').value;
    const rows = Array.from($('tester-rows').querySelectorAll('[data-tester]'));
    if (document.getElementById('testers').dataset.valid === 'false' && rows.length === 0) {
      $('list-count').textContent = '인증 기록을 확인할 수 없습니다';
      $('previous-page').disabled = true; $('next-page').disabled = true;
      $('filter-empty').hidden = true;
      return;
    }
    const selected = rows.filter(row => row.querySelector('.email-full').textContent.toLocaleLowerCase().includes(query) && (period === 'all' || row.dataset[period] === 'true'));
    selected.sort((a, b) => order === 'email' ? a.querySelector('.email-full').textContent.localeCompare(b.querySelector('.email-full').textContent) : Number(b.dataset[order]) - Number(a.dataset[order]));
    const pages = Math.max(1, Math.ceil(selected.length / pageSize));
    page = Math.min(page, pages);
    rows.forEach(row => { row.hidden = true; });
    selected.forEach((row, index) => { $('tester-rows').append(row); row.hidden = index < (page - 1) * pageSize || index >= page * pageSize; });
    $('filter-empty').hidden = selected.length > 0 || rows.length === 0;
    $('list-count').textContent = `전체 ${rows.length}명 중 ${selected.length}명 · ${page} / ${pages}쪽`;
    $('previous-page').disabled = page <= 1;
    $('next-page').disabled = page >= pages;
  }
  function chart() {
    const range = Number($('growth-range').value);
    const rows = Array.from($('join-chart').children);
    if (rows.some(row => !row.classList.contains('chart-row'))) { $('chart-total').textContent = '기록을 확인한 뒤 표시합니다'; return; }
    rows.forEach((row, i) => { row.hidden = i < rows.length - range; });
    const sum = rows.slice(-range).reduce((total, row) => total + Number(row.dataset.count), 0);
    $('chart-total').textContent = `선택한 ${range}일 · 신규 인증 ${sum}명`;
  }
  function status(text, error = false) {
    $('refresh-status').textContent = text;
    $('refresh-status').setAttribute('role', error ? 'alert' : 'status');
  }
  function applyPage(text) {
    const next = new DOMParser().parseFromString(text, 'text/html');
    if (next.title !== 'TeamHJD Game Tester 관리' || !next.getElementById('tester-rows')) throw new Error('Unexpected response');
    const testersValid = next.getElementById('testers').dataset.valid !== 'false';
    document.querySelectorAll('[data-live]').forEach(current => {
      if (!testersValid && ['tester-metrics', 'activity-metrics', 'join-chart', 'tester-rows', 'csv-controls'].includes(current.id)) return;
      const replacement = next.getElementById(current.id);
      if (replacement) current.replaceChildren(...Array.from(replacement.childNodes).map(node => document.importNode(node, true)));
    });
    if (testersValid) $('testers').dataset.valid = 'true';
    filter(); chart();
  }
  async function refresh(form = null) {
    if (busy) return;
    busy = true;
    $('refresh-now').disabled = true;
    status(form ? '관리 작업을 처리하고 있습니다…' : '최신 상태를 확인하고 있습니다…');
    try {
      const options = { credentials: 'same-origin', cache: 'no-store', signal: AbortSignal.timeout(130000) };
      if (form) { options.method = 'POST'; options.body = new URLSearchParams(new FormData(form)); }
      const response = await fetch(form ? '/control' : '/', options);
      const text = await response.text();
      if (response.status === 401) throw new Error('session');
      applyPage(text);
      if (!response.ok) throw new Error('operation');
      status(document.querySelector('#data-warning [role="alert"]') ? '인증 기록 갱신 실패 · 이전 테스터 데이터를 유지합니다' : paused ? '자동 갱신 일시 정지 · 수동 갱신 완료' : '30초마다 자동 갱신 · 최신 상태입니다');
    } catch (error) {
      status(error.message === 'session' ? '관리 세션을 확인할 수 없습니다. 바탕화면 바로가기로 다시 여세요.' : '갱신 또는 작업 완료를 확인하지 못했습니다. 잠시 후 다시 확인하세요.', true);
    } finally { busy = false; $('refresh-now').disabled = false; }
  }
  document.addEventListener('click', event => {
    const navigation = event.target.closest('[data-nav]');
    if (navigation) {
      if (navigation.dataset.period) { $('tester-search').value = ''; $('tester-period').value = navigation.dataset.period; page = 1; filter(); }
      view(navigation.dataset.nav);
    }
    const detail = event.target.closest('[data-person-detail]');
    if (detail) {
      const cells = detail.closest('tr').querySelectorAll('td');
      $('person-name').textContent = cells[0].querySelector('.email-full').textContent;
      $('person-fields').replaceChildren();
      ['최근 활동', '최초 인증 확인', '최근 새 세션', '최근 요청', '관측 세션 수'].forEach((label, i) => {
        const dt = document.createElement('dt'), dd = document.createElement('dd');
        dt.textContent = label; dd.textContent = cells[i + 1].textContent;
        $('person-fields').append(dt, dd);
      });
      $('person-dialog').showModal();
    }
  });
  document.addEventListener('submit', event => {
    const form = event.target;
    if (!form.matches('form[data-control]')) return;
    event.preventDefault();
    if (busy) return;
    const action = form.elements.action.value;
    if (['stop', 'restart', 'auto-disable'].includes(action)) {
      pendingForm = form;
      $('confirm-title').textContent = { stop: '서버를 중지할까요?', restart: '서버를 다시 시작할까요?', 'auto-disable': '로그인 자동 시작을 끌까요?' }[action];
      $('confirm-message').textContent = action === 'auto-disable' ? '다음 Windows 로그인부터 자동으로 시작하지 않습니다. 현재 서버는 계속 실행됩니다.' : '진행 중인 게임 다운로드가 끊길 수 있습니다. 관리 화면과 로그인 자동 시작 설정은 유지됩니다.';
      $('confirm-dialog').showModal();
    } else refresh(form);
  });
  $('confirm-apply').addEventListener('click', () => { const form = pendingForm; pendingForm = null; $('confirm-dialog').close(); if (form) refresh(form); });
  $('refresh-now').addEventListener('click', () => refresh());
  $('pause-refresh').addEventListener('click', () => {
    paused = !paused; $('pause-refresh').setAttribute('aria-pressed', String(paused));
    $('pause-refresh').textContent = paused ? '자동 갱신 재개' : '자동 갱신 일시 정지';
    status(paused ? '자동 갱신 일시 정지' : '30초마다 자동 갱신');
  });
  ['tester-search', 'tester-period', 'tester-sort'].forEach(id => $(id).addEventListener(id === 'tester-search' ? 'input' : 'change', () => { page = 1; filter(); }));
  $('reset-filters').addEventListener('click', () => { $('tester-search').value = ''; $('tester-period').value = 'all'; $('tester-sort').value = 'last'; page = 1; filter(); });
  $('previous-page').addEventListener('click', () => { page--; filter(); });
  $('next-page').addEventListener('click', () => { page++; filter(); });
  $('growth-range').addEventListener('change', chart);
  $('reveal-emails').addEventListener('click', () => {
    const reveal = document.documentElement.classList.toggle('reveal-emails');
    $('reveal-emails').setAttribute('aria-pressed', String(reveal));
    $('reveal-emails').textContent = reveal ? '이메일 가리기' : '이메일 전체 보기';
  });
  document.querySelector('.view-tabs').addEventListener('keydown', event => {
    const links = Array.from(document.querySelectorAll('.view-tabs a'));
    const current = links.indexOf(document.activeElement);
    if (current < 0 || !['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
    event.preventDefault();
    const index = event.key === 'Home' ? 0 : event.key === 'End' ? links.length - 1 : (current + (event.key === 'ArrowRight' ? 1 : -1) + links.length) % links.length;
    links[index].focus(); links[index].click();
  });
  window.addEventListener('hashchange', () => view(location.hash.slice(1)));
  view(location.hash.slice(1)); filter(); chart();
  setInterval(() => { if (!paused && !busy && !document.hidden && !document.querySelector('dialog[open]')) refresh(); }, 30000);
})();
