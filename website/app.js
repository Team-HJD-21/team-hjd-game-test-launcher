const translations = {
  ko: {
    faqMaintenanceQuestion: "기존 ZIP 사용자 전환·자동 업데이트·제거는?",
    faqMaintenanceAnswer: "기존 사용자는 런처를 종료하고 새 Setup을 한 번 설치하거나, Mac의 새 앱을 응용 프로그램 폴더로 옮기세요. 같은 사용자 계정의 게임과 설정을 이어서 사용합니다. 예전 작업표시줄 고정을 해제하고 설치된 TeamHJDLauncher를 다시 고정하세요.",
    faqUpdates: "이후 런처는 시작할 때와 실행 중 자동으로 확인하고, 새 버전을 검증한 뒤 자동 재시작합니다. 실패하면 재시도하거나 이전 버전으로 복원합니다. 인터넷 연결 전에는 새 파일을 받을 수 없으며 설치된 게임은 오프라인 실행을 선택할 수 있습니다. 게임 버전은 업데이트 버튼으로 직접 적용합니다.",
    faqRemoval: "Windows는 설정 → 앱 → 설치된 앱에서 제거합니다. Mac은 응용 프로그램의 TeamHJDLauncher.app을 휴지통으로 옮깁니다. 게임·설정·로그인 정보·Unity 저장 데이터와 서버 기록은 보존됩니다. 로그인 정보는 런처 파일 메뉴에서, 게임은 게임 관리에서 별도로 삭제할 수 있습니다.",
    faqRepair: "파일이 손상되면 최신 Setup을 다시 실행하거나 새 DMG의 앱으로 교체하세요. 프로그램 제거 후 재설치도 가능합니다. 서버 기록의 삭제·동의 철회는 support@teamhjd.com으로 요청하세요.",
    portable: "설치 없이 ZIP이 필요한 경우",

    skip: '다운로드로 바로 가기', language: '언어', headerHome: '회사 홈페이지',
    intro: 'TeamHJD의 게임 플레이테스트를 위한 런처입니다.',
    details: '이메일 인증 후 테스트 게임을 설치하고 실행할 수 있습니다. 새 버전 확인과 업데이트, 설치 파일 관리도 런처에서 진행할 수 있습니다.',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 15 이상', download: '다운로드',
    runHint: 'Windows는 Setup.exe를 실행하세요. Mac은 DMG를 열고 앱을 응용 프로그램 폴더로 옮기세요.',
    collectionNotice: '런처의 이메일 인증 시 인증된 이메일 주소와 인증·접속 시각이 수집·보관됩니다.',
    faqTitle: '첫 실행 FAQ',
    faqIntro: '현재 테스트 배포본은 Windows 게시자 서명과 Apple Developer ID 서명·공증이 없어, 처음 실행할 때 보안 경고가 나타날 수 있습니다.',
    faqTrust: '이 페이지에서 연결한 공식 GitHub 파일을 받았고 출처를 신뢰하는 경우에만 아래 절차를 진행하세요.',
    faqWindowsQuestion: '‘Windows의 PC 보호’가 표시되나요?',
    faqWindowsStep1: '공식 Setup.exe를 실행하세요. 관리자 권한 없이 현재 Windows 계정에 설치합니다.',
    faqWindowsStep2: 'SmartScreen 경고 창에서 ‘추가 정보’를 누르세요.',
    faqWindowsStep3: '앱 이름이 TeamHJD-GameTestLauncher-Windows-x64-Setup.exe인지 확인하고 ‘실행’을 선택하세요.',
    faqWindowsNote: '게시자가 ‘알 수 없는 게시자’로 표시될 수 있습니다. ‘실행’ 버튼이 없거나 Smart App Control·조직 정책으로 차단되면 아래 공통 안내를 확인하세요.',
    faqMicrosoft: 'Microsoft 안내 ↗',
    faqMacQuestion: '개발자를 확인할 수 없어 열리지 않나요?',
    faqMacStep1: 'DMG를 열고 TeamHJDLauncher.app을 응용 프로그램 폴더로 옮긴 뒤 한 번 실행하세요. 개발자 확인·Apple 검사 관련 경고가 뜨면 닫으세요.',
    faqMacStep2: '‘시스템 설정 → 개인정보 보호 및 보안’을 여세요.',
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
    faqMaintenanceQuestion: "Migrating from ZIP, updates and uninstalling",
    faqMaintenanceAnswer: "Close the old launcher and run Setup once, or replace the Mac app in Applications. Your existing games and settings for the same user account are retained. Remove old taskbar pins and pin the installed TeamHJDLauncher.",
    faqUpdates: "The launcher checks on startup and periodically, verifies updates and restarts automatically. Failures retry or restore the previous version. New files need an internet connection; installed games can be played offline. Game updates require the Update button.",
    faqRemoval: "Windows: Settings → Apps → Installed apps. Mac: move TeamHJDLauncher.app from Applications to Trash. Games, settings, credentials, Unity saves and server records are retained. Remove local credentials in the File menu and games in game management.",
    faqRepair: "Repair by running the latest Setup again or replacing the app from a new DMG. Reinstallation is also supported. Request server record deletion or consent withdrawal at support@teamhjd.com.",
    portable: "Need a portable ZIP instead?",

    skip: 'Skip to downloads', language: 'Language', headerHome: 'Company website',
    intro: 'A launcher for TeamHJD game playtests.',
    details: 'Verify your email to install and run test games. The launcher also lets you check for new versions, apply updates and manage installed files.',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 15 or later', download: 'Download',
    runHint: 'Windows: run Setup.exe. Mac: open the DMG and drag the app to Applications.',
    collectionNotice: 'When you verify your email in the launcher, your verified email address and authentication and access times are collected and stored.',
    faqTitle: 'First-launch FAQ',
    faqIntro: 'This test build has no Windows publisher signature or Apple Developer ID signature and notarization. A security warning may appear when you first open it.',
    faqTrust: 'Continue only if you downloaded the official GitHub file linked on this page and trust its source.',
    faqWindowsQuestion: 'Seeing “Windows protected your PC”?',
    faqWindowsStep1: 'Run the official Setup.exe. It installs for your Windows account without administrator privileges.',
    faqWindowsStep2: 'In the SmartScreen warning, select “More info”.',
    faqWindowsStep3: 'Check the app name is TeamHJD-GameTestLauncher-Windows-x64-Setup.exe, then select “Run anyway”.',
    faqWindowsNote: 'The publisher may appear as “Unknown publisher”. If “Run anyway” is missing, or Smart App Control or an organization policy blocks it, see the general guidance below.',
    faqMicrosoft: 'Microsoft guidance ↗',
    faqMacQuestion: 'Blocked because the developer cannot be verified?',
    faqMacStep1: 'Open the DMG, move TeamHJDLauncher.app to Applications and try opening it once. Dismiss the developer verification or Apple checking warning.',
    faqMacStep2: 'Open System Settings → Privacy & Security.',
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
    faqMaintenanceQuestion: "旧 ZIP 迁移、更新与卸载",
    faqMaintenanceAnswer: "关闭旧启动器，运行一次 Setup；Mac 用户请替换“应用程序”中的应用。同一用户账户的游戏和设置会保留。取消固定旧图标，并重新固定已安装的 TeamHJDLauncher。",
    faqUpdates: "启动器会在启动时及运行中定期检查，验证更新后自动重启。失败时重试或恢复旧版本。下载需要网络；已安装游戏可选择离线运行。游戏更新需点击“更新”按钮。",
    faqRemoval: "Windows：设置 → 应用 → 已安装的应用。Mac：将“应用程序”中的 TeamHJDLauncher.app 移到废纸篓。游戏、设置、登录信息、Unity 存档和服务器记录会保留。可在文件菜单删除本地登录信息，在游戏管理中删除游戏。",
    faqRepair: "重新运行最新 Setup，或用新 DMG 的应用替换以修复文件。也可卸载后重新安装。删除服务器记录或撤回同意请联系 support@teamhjd.com。",
    portable: "需要免安装 ZIP？",

    skip: '跳转到下载', language: '语言', headerHome: '公司主页',
    intro: '用于 TeamHJD 游戏测试的启动器。',
    details: '通过邮箱验证后，即可安装并运行测试游戏。您还可以在启动器中检查新版本、更新游戏和管理安装文件。',
    windowsRequirement: 'Windows 10 / 11 · x64', macRequirement: 'macOS 15 或更高版本', download: '下载',
    runHint: 'Windows：运行 Setup.exe。Mac：打开 DMG，将应用拖到“应用程序”。',
    collectionNotice: '在启动器中进行邮箱验证时，已验证的邮箱地址以及验证和访问时间会被收集并保存。',
    faqTitle: '首次运行 FAQ',
    faqIntro: '当前测试版本没有 Windows 发布者签名，也没有 Apple Developer ID 签名和公证，首次运行时可能出现安全警告。',
    faqTrust: '仅在您通过本页链接下载了官方 GitHub 文件并信任其来源时，才继续以下步骤。',
    faqWindowsQuestion: '出现“Windows 已保护你的电脑”？',
    faqWindowsStep1: '运行官方 Setup.exe。无需管理员权限，安装到当前 Windows 账户。',
    faqWindowsStep2: '在 SmartScreen 警告中选择“更多信息”。',
    faqWindowsStep3: '确认应用名称是 TeamHJD-GameTestLauncher-Windows-x64-Setup.exe，然后选择“仍要运行”。',
    faqWindowsNote: '发布者可能显示为“未知发布者”。如果没有“仍要运行”按钮，或被智能应用控制、组织策略阻止，请查看下方的通用说明。',
    faqMicrosoft: 'Microsoft 官方说明 ↗',
    faqMacQuestion: '因无法验证开发者而打不开？',
    faqMacStep1: '打开 DMG，将 TeamHJDLauncher.app 移到“应用程序”文件夹并尝试打开一次。出现开发者验证或 Apple 检查警告时，关闭提示。',
    faqMacStep2: '打开“系统设置 → 隐私与安全性”。',
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
const languageControl = document.querySelector('#language-switcher');
const languageLabels = {ko:'한국어', en:'English', 'zh-CN':'简体中文'};

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
  document.querySelector('#current-language').textContent = languageLabels[language];
  for (const button of languageControl.querySelectorAll('[data-language]')) {
    button.setAttribute('aria-pressed', String(button.dataset.language === language));
  }
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
for (const button of languageControl.querySelectorAll('[data-language]')) {
  button.addEventListener('click', () => {
    setLanguage(button.dataset.language, true);
    languageControl.open = false;
    languageControl.querySelector('summary').focus();
  });
}

const relatedSites = document.querySelector('#related-sites');
const dropdowns = [languageControl, relatedSites];
for (const dropdown of dropdowns) {
  dropdown.addEventListener('toggle', () => {
    if (dropdown.open) for (const other of dropdowns) if (other !== dropdown) other.open = false;
  });
  dropdown.addEventListener('focusout', event => {
    if (event.relatedTarget && !dropdown.contains(event.relatedTarget)) dropdown.open = false;
  });
}
document.addEventListener('click', event => {
  for (const dropdown of dropdowns) {
    if (dropdown.open && !dropdown.contains(event.target)) dropdown.open = false;
  }
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') for (const dropdown of dropdowns) {
    if (dropdown.open) {
      dropdown.open = false;
      dropdown.querySelector('summary').focus();
    }
  }
});

async function loadRelease() {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 8000);
  try {
    const response = await fetch('updates/stable.json?check='+Date.now(), {
      credentials:'omit', referrerPolicy:'no-referrer', cache:'no-store', signal:controller.signal
    });
    if(!response.ok || Number(response.headers.get('content-length'))>256*1024) return;
    const text=await response.text();if(text.length>256*1024)return;
    let minimum=0;
    try { minimum=Number(localStorage.getItem('teamhjd-release-sequence'))||0; } catch {}
    const candidate=await TeamHJDFeed.verify(JSON.parse(text),minimum);
    for(const [button,platform] of [['windows','windows-x64'],['arm64','macos-arm64'],['x64','macos-x64']]) {
      document.querySelector(`[data-download="${button}"]`).href=candidate.platforms[platform].installer.url;
    }
    try { localStorage.setItem('teamhjd-release-sequence',String(candidate.sequence)); } catch {}
    release={tag_name:'v'+candidate.version,published_at:candidate.publishedAt};
    showRelease();
  } catch { /* Static official latest/download links remain available. */ }
  finally {clearTimeout(timeout);}
}
loadRelease();
setInterval(loadRelease,300000);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)loadRelease()});
