'use strict';

const releaseApi = 'https://api.github.com/repos/Team-HJD-21/team-hjd-game-test-launcher/releases/latest';
const assetNames = {
  windows: 'TeamHJD-GameTestLauncher-Windows-x64.zip',
  arm64: 'TeamHJD-GameTestLauncher-macOS-arm64.zip',
  x64: 'TeamHJD-GameTestLauncher-macOS-x64.zip'
};
const ko = Object.fromEntries([...document.querySelectorAll('[data-i18n]')].map(node => [node.dataset.i18n, node.textContent]));
ko.intro = '게임을 만나기까지, 더 간편하게.\n내 컴퓨터에 맞는 런처를 받고 이메일 인증으로 시작하세요.';
const translations = {
  ko,
  en: {
    skip: 'Skip to downloads', home: 'TeamHJD home ↗', language: 'Language',
    title1: 'Your next playtest.', title2: 'Start here.',
    intro: 'Less setup. More time to play.\nDownload the launcher for your computer and start with email verification.',
    stable: 'LATEST RELEASE', releaseHint: 'Downloads always point to the latest GitHub release.',
    choose: 'Choose your platform.', windowsRequirement: 'Windows 10 / 11 · 64-bit',
    windowsDownload: 'Download for Windows', windowsPackage: 'Windows x64',
    macRequirement: 'macOS 12 or later · Choose your chip', appleHint: 'M1 and later', intelHint: 'Intel-based Macs',
    macChip: 'Find your chip in Apple menu → About This Mac.',
    downloadNote: 'Official ZIP files are downloaded from GitHub. No email verification is needed to download the launcher.',
    startTitle: 'Ready in three steps.', step1Title: 'Extract and launch',
    step1: 'On Windows, extract the whole folder and run TeamHJDLauncher.exe. On Mac, move TeamHJDLauncher.app to Applications and open it.',
    step2Title: 'Verify your email', step2: 'Select “Start with email” in the launcher. Enter your email and the code in your browser, then return to the launcher.',
    step3Title: 'Install and play', step3: 'Install a game and jump in. The launcher notifies you about new game versions; choose Update when you are ready.',
    helpTitle: 'A little help getting started.', faq1Title: 'macOS is blocking the app.',
    faq1: 'This playtest build is not notarized by Apple. Verify that you downloaded the official release, then use System Settings → Privacy & Security → Open Anyway for this app. Your first sign-in may also request Keychain access.',
    faq2Title: 'Should I keep every file in the folder?', faq2: 'Yes. Keep the complete extracted Windows folder together. Moving only the EXE will leave required DLLs behind. Use TeamHJDLauncher for shortcuts and pinned icons.',
    faq3Title: 'Do I need to download updates again?', faq3: 'The launcher prepares its own updates automatically and applies them on the next launch. For games, select Update to install a new version, or keep playing your installed version.',
    guide: 'Read the launcher guide ↗', privacy: 'Privacy policy', businessName: 'Business name', businessOwner: 'Representative',
    businessRegistration: 'Business registration', businessVerify: '(Verify business)', businessCommerce: 'Mail-order registration', businessAddress: 'Business address',
    press: 'Press kit', contentPolicy: 'Content & AI policy', aiPolicy: 'AI search and retrieval are permitted. Use of this content for AI training is prohibited.',
    related: 'Related sites', mainSite: 'TeamHJD website ↗', docs: 'Team documentation ↗'
  },
  'zh-CN': {
    skip: '跳转到下载', home: 'TeamHJD 官网 ↗', language: '语言',
    title1: '下一次游戏测试，', title2: '从这里开始。',
    intro: '减少准备，尽早开始游戏。\n下载适合电脑的启动器，通过邮箱验证即可开始。',
    stable: 'LATEST RELEASE', releaseHint: '下载链接始终指向 GitHub 上的最新版本。',
    choose: '选择适合电脑的版本。', windowsRequirement: 'Windows 10 / 11 · 64 位',
    windowsDownload: '下载 Windows 版', windowsPackage: 'Windows x64',
    macRequirement: 'macOS 12 或更高版本 · 按芯片选择', appleHint: 'M1 及后续机型', intelHint: 'Intel 芯片机型',
    macChip: '在苹果菜单 → 关于本机中查看芯片。',
    downloadNote: '从 GitHub 下载官方 ZIP 文件。下载启动器无需邮箱验证。',
    startTitle: '三步即可开始。', step1Title: '解压并运行',
    step1: 'Windows 用户请解压整个文件夹后运行 TeamHJDLauncher.exe。Mac 用户请将 TeamHJDLauncher.app 移入“应用程序”后打开。',
    step2Title: '验证邮箱', step2: '在启动器中选择通过邮箱开始，在浏览器中输入邮箱和收到的验证码，然后返回启动器。',
    step3Title: '安装并开始游戏', step3: '安装游戏后即可开始。启动器会提示游戏新版本，请在准备好后选择更新。',
    helpTitle: '开始之前的常见问题。', faq1Title: 'macOS 阻止打开应用。',
    faq1: '目前的测试版本尚未经过 Apple 公证。确认文件来自官方发布页后，在系统设置 → 隐私与安全性中为此应用选择“仍要打开”。首次登录时还可能需要允许访问钥匙串。',
    faq2Title: '需要保留文件夹中的全部文件吗？', faq2: '需要。Windows 用户请保留完整的解压文件夹。仅移动 EXE 会丢失必需的 DLL。创建快捷方式或固定图标时请选择 TeamHJDLauncher。',
    faq3Title: '更新时需要重新下载吗？', faq3: '启动器会自动准备自身更新，并在下次启动时应用。游戏新版本则由您选择更新，也可以继续游玩已安装的版本。',
    guide: '查看启动器使用指南 ↗', privacy: '隐私政策', businessName: '商号', businessOwner: '代表人',
    businessRegistration: '营业登记号', businessVerify: '（查询企业信息）', businessCommerce: '通信销售申报号', businessAddress: '营业地址',
    press: '媒体资料', contentPolicy: '内容使用与 AI 政策', aiPolicy: '允许 AI 搜索与信息检索，禁止将内容用于 AI 训练。',
    related: '相关网站', mainSite: 'TeamHJD 官网 ↗', docs: '团队开发文档 ↗'
  }
};

let release = null;
let language = 'ko';
const languageControl = document.querySelector('#language');
const locale = () => ({ko: 'ko-KR', en: 'en-US', 'zh-CN': 'zh-CN'})[language];

function showRelease() {
  if (!release) return;
  document.querySelector('#release-version').textContent = release.tag_name;
  const date = new Date(release.published_at);
  document.querySelector('#release-date').textContent = Number.isFinite(date.getTime())
    ? new Intl.DateTimeFormat(locale(), {year: 'numeric', month: 'short', day: 'numeric'}).format(date)
    : translations[language].releaseHint;
  for (const [platform, name] of Object.entries(assetNames)) {
    const asset = release.assets.find(item => item.name === name);
    const label = document.querySelector(`[data-size="${platform}"]`);
    if (label) {
      const prefix = {windows: '', arm64: 'Apple Silicon ', x64: 'Intel '}[platform];
      label.textContent = `${prefix}${new Intl.NumberFormat(locale(), {maximumFractionDigits: 1}).format(asset.size / 1024 ** 2)} MB`;
    }
  }
}

function setLanguage(value, persist = false) {
  language = Object.hasOwn(translations, value) ? value : 'ko';
  document.documentElement.lang = language;
  languageControl.value = language;
  for (const node of document.querySelectorAll('[data-i18n]')) {
    node.textContent = translations[language][node.dataset.i18n] ?? ko[node.dataset.i18n];
  }
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

document.addEventListener('click', event => {
  for (const menu of document.querySelectorAll('.related-sites[open]')) {
    if (!menu.contains(event.target)) menu.open = false;
  }
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') {
    for (const menu of document.querySelectorAll('.related-sites[open]')) {
      menu.open = false;
      menu.querySelector('summary').focus();
    }
  }
});

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
    for (const name of Object.values(assetNames)) {
      const matches = candidate.assets.filter(asset => asset.name === name);
      const expected = `https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/${candidate.tag_name}/${name}`;
      if (matches.length !== 1 || matches[0].browser_download_url !== expected || !Number.isSafeInteger(matches[0].size) || matches[0].size <= 0) return;
    }
    release = candidate;
    showRelease();
  } catch {
    // Static latest/download links work without JavaScript or API availability.
  } finally {
    clearTimeout(timeout);
  }
}
loadRelease();
