const releaseApi = 'https://api.github.com/repos/Team-HJD-21/team-hjd-game-test-launcher/releases/latest';
const assetNames = [
  'TeamHJD-GameTestLauncher-Windows-x64.zip',
  'TeamHJD-GameTestLauncher-macOS-arm64.zip',
  'TeamHJD-GameTestLauncher-macOS-x64.zip'
];
const translations = {
  ko: {
    skip: '다운로드로 바로 가기', language: '언어',
    intro: '게임 플레이테스트를 위한 런처입니다.',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 12 이상', download: '다운로드',
    collectionNotice: '런처의 이메일 인증 시 인증된 이메일 주소와 인증·접속 시각이 수집·보관됩니다.',
    home: 'TeamHJD 홈', privacy: '개인정보처리방침'
  },
  en: {
    skip: 'Skip to downloads', language: 'Language',
    intro: 'A launcher for game playtests.',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 12 or later', download: 'Download',
    collectionNotice: 'When you verify your email in the launcher, your verified email address and authentication and access times are collected and stored.',
    home: 'TeamHJD home', privacy: 'Privacy policy'
  },
  'zh-CN': {
    skip: '跳转到下载', language: '语言',
    intro: '用于游戏测试的启动器。',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 12 或更高版本', download: '下载',
    collectionNotice: '在启动器中进行邮箱验证时，已验证的邮箱地址以及验证和访问时间会被收集并保存。',
    home: 'TeamHJD 官网', privacy: '隐私政策'
  }
};
let release = null;
let language = 'ko';
const languageControl = document.querySelector('#language');

function showRelease() {
  if (!release) return;
  document.querySelector('#release-version').textContent = release.tag_name;
  const date = document.querySelector('#release-date');
  date.dateTime = release.published_at;
  date.textContent = new Intl.DateTimeFormat({ko:'ko-KR', en:'en-US', 'zh-CN':'zh-CN'}[language],
    {year:'numeric', month:'short', day:'numeric'}).format(new Date(release.published_at));
  document.querySelector('#release-info').hidden = false;
}

function setLanguage(value, persist = false) {
  language = Object.hasOwn(translations, value) ? value : 'ko';
  document.documentElement.lang = language;
  languageControl.value = language;
  for (const node of document.querySelectorAll('[data-i18n]')) {
    node.textContent = translations[language][node.dataset.i18n];
  }
  document.querySelector('meta[name="description"]').content = translations[language].intro;
  for (const link of document.querySelectorAll('[data-home]')) {
    link.href = `https://teamhjd.com/?lang=${encodeURIComponent(language)}`;
  }
  const url = new URL(location.href);
  url.searchParams.set('lang', language);
  history.replaceState(null, '', url);
  if (persist) {
    try { localStorage.setItem('teamhjd-launcher-language', language); } catch { /* Storage is optional. */ }
  }
  showRelease();
}

let initialLanguage = new URL(location.href).searchParams.get('lang');
if (!initialLanguage) {
  try { initialLanguage = localStorage.getItem('teamhjd-launcher-language'); } catch { /* Keep Korean default. */ }
}
setLanguage(initialLanguage);
languageControl.addEventListener('change', event => setLanguage(event.target.value, true));

async function loadRelease() {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 8000);
  try {
    const response = await fetch(releaseApi, {
      headers: {Accept: 'application/vnd.github+json'},
      credentials: 'omit', referrerPolicy: 'no-referrer', cache: 'no-cache', signal: controller.signal
    });
    if (!response.ok) return;
    const candidate = await response.json();
    if (candidate.draft || candidate.prerelease || !/^v[0-9]+\.[0-9]+\.[0-9]+$/.test(candidate.tag_name) || !Array.isArray(candidate.assets)) return;
    if (typeof candidate.published_at !== 'string' || !Number.isFinite(new Date(candidate.published_at).getTime())) return;
    for (const name of assetNames) {
      const matches = candidate.assets.filter(asset => asset.name === name);
      const expected = `https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/${candidate.tag_name}/${name}`;
      if (matches.length !== 1 || matches[0].browser_download_url !== expected || !Number.isSafeInteger(matches[0].size) || matches[0].size <= 0) return;
    }
    release = candidate;
    showRelease();
  } catch {
    // The static latest/download links also work without metadata or JavaScript.
  } finally {
    clearTimeout(timeout);
  }
}
loadRelease();
