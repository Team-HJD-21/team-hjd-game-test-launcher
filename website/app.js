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
    faqTitle: '첫 실행 FAQ',
    faqIntro: '현재 테스트 배포본은 Windows 게시자 서명과 Apple Developer ID 서명·공증이 없어, 처음 실행할 때 보안 경고가 나타날 수 있습니다.',
    faqTrust: '이 페이지에서 연결한 공식 GitHub 파일을 받았고 출처를 신뢰하는 경우에만 아래 절차를 진행하세요.',
    faqWindowsQuestion: '‘Windows의 PC 보호’가 표시되나요?',
    faqWindowsStep1: 'ZIP 전체를 일반 폴더에 푼 뒤, 그 안의 TeamHJDLauncher.exe를 실행하세요.',
    faqWindowsStep2: 'SmartScreen 경고 창에서 ‘추가 정보’를 누르세요.',
    faqWindowsStep3: '앱 이름이 TeamHJDLauncher.exe인지 확인하고, 실행하려면 ‘실행’을 선택하세요.',
    faqWindowsNote: '게시자가 ‘알 수 없는 게시자’로 표시될 수 있습니다. ‘실행’ 버튼이 없거나 Smart App Control·조직 정책으로 차단되면 아래 공통 안내를 확인하세요.',
    faqMicrosoft: 'Microsoft 안내 ↗',
    faqMacQuestion: '개발자를 확인할 수 없어 열리지 않나요?',
    faqMacStep1: 'ZIP을 풀고 TeamHJDLauncher.app을 응용 프로그램 폴더로 옮긴 뒤 한 번 실행하세요. 개발자 확인·Apple 검사 관련 경고가 뜨면 닫으세요.',
    faqMacStep2: 'macOS 13 이상은 ‘시스템 설정 → 개인정보 보호 및 보안’을 여세요. macOS 12는 ‘시스템 환경설정 → 보안 및 개인 정보 보호 → 일반’에서 확인하세요.',
    faqMacStep3: 'TeamHJDLauncher의 차단 안내 옆 ‘그래도 열기’를 누르고, 확인 창에서 ‘열기’를 선택하세요. Mac에서 인증을 요청하면 해당 시스템 창에서 승인하세요.',
    faqMacNote: '‘그래도 열기’가 보이지 않으면 앱을 다시 한 번 실행한 직후 설정을 확인하세요. OS 버전에 따라 문구와 버튼 위치가 다를 수 있습니다.',
    faqApple: 'Apple 안내 ↗', faqCommon: '공통',
    faqBlockedQuestion: '계속 차단되거나 ‘손상됨’ 경고가 뜨나요?',
    faqBlockedDamage: '‘악성코드’, ‘컴퓨터를 손상시킴’, ‘앱이 손상됨’ 경고는 단순한 개발자 확인 경고와 다릅니다. 실행을 중단하고 이 페이지에서 새로 받은 파일로도 같은 경고가 뜨는지 확인한 뒤 문의해 주세요.',
    faqBlockedPolicy: '실행 버튼이 없거나 회사·학교 PC의 정책으로 차단되면 담당 관리자에게 문의하세요. 백신·SmartScreen·Gatekeeper 전체를 끄는 방식은 사용하지 마세요.',
    faqContact: '경고 화면과 OS 버전을 첨부해 문의하기 ↗',
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
    faqTitle: 'First-launch FAQ',
    faqIntro: 'This test build has no Windows publisher signature or Apple Developer ID signature and notarization. A security warning may appear when you first open it.',
    faqTrust: 'Continue only if you downloaded the official GitHub file linked on this page and trust its source.',
    faqWindowsQuestion: 'Seeing “Windows protected your PC”?',
    faqWindowsStep1: 'Extract the entire ZIP into a regular folder, then open TeamHJDLauncher.exe inside it.',
    faqWindowsStep2: 'In the SmartScreen warning, select “More info”.',
    faqWindowsStep3: 'Check that the app name is TeamHJDLauncher.exe. To proceed, select “Run anyway”.',
    faqWindowsNote: 'The publisher may appear as “Unknown publisher”. If “Run anyway” is missing, or Smart App Control or an organization policy blocks it, see the general guidance below.',
    faqMicrosoft: 'Microsoft guidance ↗',
    faqMacQuestion: 'Blocked because the developer cannot be verified?',
    faqMacStep1: 'Extract the ZIP, move TeamHJDLauncher.app to Applications and try opening it once. Dismiss the developer verification or Apple checking warning.',
    faqMacStep2: 'On macOS 13 or later, open System Settings → Privacy & Security. On macOS 12, open System Preferences → Security & Privacy → General.',
    faqMacStep3: 'Select “Open Anyway” for TeamHJDLauncher, then “Open” in the confirmation. Approve any authentication request in the Mac system dialog.',
    faqMacNote: 'If “Open Anyway” is missing, try opening the app again and check settings immediately afterward. Wording and button locations may vary by OS version.',
    faqApple: 'Apple guidance ↗', faqCommon: 'General',
    faqBlockedQuestion: 'Still blocked, or seeing an “app is damaged” warning?',
    faqBlockedDamage: 'Malware, “will damage your computer” and “app is damaged” warnings differ from developer verification warnings. Stop running the app. Download a fresh copy from this page and contact us if the warning persists.',
    faqBlockedPolicy: 'If the run button is unavailable or a work or school policy blocks the app, contact your administrator. Do not turn off antivirus, SmartScreen or Gatekeeper system-wide.',
    faqContact: 'Contact us with the warning screenshot and OS version ↗',
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
    faqTitle: '首次运行 FAQ',
    faqIntro: '当前测试版本没有 Windows 发布者签名，也没有 Apple Developer ID 签名和公证，首次运行时可能出现安全警告。',
    faqTrust: '仅在您通过本页链接下载了官方 GitHub 文件并信任其来源时，才继续以下步骤。',
    faqWindowsQuestion: '出现“Windows 已保护你的电脑”？',
    faqWindowsStep1: '将整个 ZIP 解压到普通文件夹，然后运行其中的 TeamHJDLauncher.exe。',
    faqWindowsStep2: '在 SmartScreen 警告中选择“更多信息”。',
    faqWindowsStep3: '确认应用名称是 TeamHJDLauncher.exe。如要继续，请选择“仍要运行”。',
    faqWindowsNote: '发布者可能显示为“未知发布者”。如果没有“仍要运行”按钮，或被智能应用控制、组织策略阻止，请查看下方的通用说明。',
    faqMicrosoft: 'Microsoft 官方说明 ↗',
    faqMacQuestion: '因无法验证开发者而打不开？',
    faqMacStep1: '解压 ZIP，将 TeamHJDLauncher.app 移到“应用程序”文件夹并尝试打开一次。出现开发者验证或 Apple 检查警告时，关闭提示。',
    faqMacStep2: 'macOS 13 及以上：打开“系统设置 → 隐私与安全性”。macOS 12：打开“系统偏好设置 → 安全性与隐私 → 通用”。',
    faqMacStep3: '在 TeamHJDLauncher 的阻止提示旁选择“仍要打开”，然后在确认窗口中选择“打开”。如 Mac 要求认证，请在系统窗口中批准。',
    faqMacNote: '若看不到“仍要打开”，请再次尝试打开应用后立即检查设置。文字和按钮位置可能因系统版本而异。',
    faqApple: 'Apple 官方说明 ↗', faqCommon: '通用',
    faqBlockedQuestion: '仍被阻止，或显示“应用已损坏”？',
    faqBlockedDamage: '恶意软件、“将损坏电脑”和“应用已损坏”警告与开发者验证警告不同。请停止运行，从本页重新下载文件；如果仍有相同警告，请联系我们。',
    faqBlockedPolicy: '如果没有运行按钮，或被公司、学校的策略阻止，请联系管理员。请勿关闭整个系统的杀毒软件、SmartScreen 或 Gatekeeper。',
    faqContact: '附上警告截图和系统版本联系我们 ↗',
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
    const text = translations[language][node.dataset.i18n] ?? translations.ko[node.dataset.i18n];
    if (typeof text === 'string') node.textContent = text;
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
