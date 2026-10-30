const releaseApi = 'https://api.github.com/repos/Team-HJD-21/team-hjd-game-test-launcher/releases/latest';
const assetNames = [
  'TeamHJD-GameTestLauncher-Windows-x64.zip',
  'TeamHJD-GameTestLauncher-macOS-arm64.zip',
  'TeamHJD-GameTestLauncher-macOS-x64.zip'
];
const translations = {
  ko: {
    skip: '다운로드로 바로 가기', language: '언어',
    intro: 'TeamHJD의 게임 플레이테스트를 위한 런처입니다.',
    details: '이메일 인증 후 테스트 게임을 설치하고 실행할 수 있습니다. 새 버전 확인과 업데이트, 설치 파일 관리도 런처에서 진행할 수 있습니다.',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 12 이상', download: '다운로드',
    runHint: 'ZIP 파일을 푼 뒤 TeamHJDLauncher를 실행하세요.',
    collectionNotice: '런처의 이메일 인증 시 인증된 이메일 주소와 인증·접속 시각이 수집·보관됩니다.',
    businessName: '상호', businessOwner: '대표자', ownerValue: '황재동',
    businessRegistration: '사업자등록번호', businessVerify: '(사업자정보확인)',
    businessCommerce: '통신판매업 신고번호', businessAddress: '사업장 주소',
    privacy: '개인정보처리방침', press: '프레스킷', contentPolicy: '콘텐츠 이용 · AI 학습 정책',
    aiPolicy: 'AI 검색·정보 조회는 허용하며, 콘텐츠의 AI 학습 이용은 금지합니다.',
    related: '관련 사이트', teamDocs: '팀 개발 문서', footerNavigation: '하단 메뉴'
  },
  en: {
    skip: 'Skip to downloads', language: 'Language',
    intro: 'A launcher for TeamHJD game playtests.',
    details: 'Verify your email to install and run test games. The launcher also lets you check for new versions, apply updates and manage installed files.',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 12 or later', download: 'Download',
    runHint: 'Extract the ZIP, then run TeamHJDLauncher.',
    collectionNotice: 'When you verify your email in the launcher, your verified email address and authentication and access times are collected and stored.',
    businessName: 'Registered business name', businessOwner: 'Business representative', ownerValue: 'HWANG JAEDONG',
    businessRegistration: 'Business registration no.', businessVerify: '(Business information)',
    businessCommerce: 'Mail-order business registration no.', businessAddress: 'Business address',
    privacy: 'Privacy policy (Korean)', press: 'Press kit', contentPolicy: 'Content use & AI training policy',
    aiPolicy: 'AI search and information retrieval are permitted. Use of our content for AI training is prohibited.',
    related: 'Related sites', teamDocs: 'Team documentation', footerNavigation: 'Footer navigation'
  },
  'zh-CN': {
    skip: '跳转到下载', language: '语言',
    intro: '用于 TeamHJD 游戏测试的启动器。',
    details: '通过邮箱验证后，即可安装并运行测试游戏。您还可以在启动器中检查新版本、更新游戏和管理安装文件。',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 12 或更高版本', download: '下载',
    runHint: '解压 ZIP 后，运行 TeamHJDLauncher。',
    collectionNotice: '在启动器中进行邮箱验证时，已验证的邮箱地址以及验证和访问时间会被收集并保存。',
    businessName: '登记名称', businessOwner: '负责人', ownerValue: 'HWANG JAEDONG',
    businessRegistration: '韩国营业登记号', businessVerify: '(查看商家信息)',
    businessCommerce: '韩国网络销售备案号', businessAddress: '营业地址',
    privacy: '隐私政策（韩文）', press: '媒体资料包', contentPolicy: '内容使用与 AI 训练政策',
    aiPolicy: '允许 AI 搜索和信息查询；禁止将本站内容用于 AI 训练。',
    related: '相关网站', teamDocs: '团队开发文档', footerNavigation: '页脚导航'
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
  document.querySelector('.footer-links').setAttribute('aria-label', translations[language].footerNavigation);
  for (const link of document.querySelectorAll('a[href]')) {
    const target = new URL(link.href, location.href);
    if (target.origin !== 'https://teamhjd.com' && !link.classList.contains('testlauncher-link')) continue;
    target.searchParams.set('lang', language);
    link.href = target.href;
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

const relatedSites = document.querySelector('#related-sites');
document.addEventListener('click', event => {
  if (relatedSites.open && !relatedSites.contains(event.target)) relatedSites.open = false;
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape' && relatedSites.open) {
    relatedSites.open = false;
    relatedSites.querySelector('summary').focus();
  }
});
relatedSites.addEventListener('focusout', event => {
  if (event.relatedTarget && !relatedSites.contains(event.relatedTarget)) relatedSites.open = false;
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
