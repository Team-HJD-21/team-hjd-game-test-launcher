# Visual rules

The product is a game library and a place to enter a team playtest. The supplied chalk/pencil TeamHJD wordmark and rocket are the brand anchors. Do not regenerate, stretch or recolor the marks. Until approved game art arrives through the private distribution, the hero uses the real team artwork with restrained rings and typography; it does not pretend to show gameplay.

- Sidebar `#111416`, surface `#171A1D`, elevated surface `#20262B`, fine separators `#30373D`.
- Primary text `#F7F6EE`, secondary `#AEB8BF`, muted labels `#849099`, CTA accent `#E8C76A`, connection `#8CBD9E`, inline error `#F0BAA2` on `#382B28`.
- Integrated 40px title bar with File/View/Help, notification history, minimize/maximize/close, native resize borders and Windows maximize hit testing. Start size 1160×790 logical pixels, minimum 1000×700. Sidebar 222, content gutters 36, hero 235–265 high. Basic controls use 4–7px corner radii, 42px buttons and 44px fields. Primary action is 164×50.
- Malgun Gothic for Korean UI and Segoe UI for the game title. Body 12–13px, section 14–16px, dialog title 22px, hero 33–45px. Long names wrap up to two lines; lists elide and retain accessible full names.
- One main action changes from connect → install → update → play → running. Checking and updates run automatically. Version/size is subordinate. The `···` menu includes full version/change information, repair, restore and uninstall with a concrete confirmation showing the game and storage scope. Server credentials and publisher key live in a scrolling modal settings surface.
- Empty/offline state never invents a build, progress, game count or connected server. Cached versions remain visible offline and an installed game can run. Errors explain a next action in Korean without stack traces, passwords or network payloads.
- Tab focus has a 2px visible border, input selection/caret uses native Qt editing behavior, buttons have hover/pressed states and pointer cursors, Escape closes settings. No long hover tooltip overlays are used.
- Initial entrance 180ms, interaction colors 90ms, progress easing 100ms. Data updates change bindings in place; panels do not disappear/reenter. Windows client-area animation preference initializes reduced motion; the user can also enable it in settings.

Review targets: actual app at 100/125/150% scale; full and minimum size; empty, settings, connected, install progress, ready, running, offline and error; long Korean/English titles and descriptions. Render verification and user approval of the art direction are separate results.

Reference: [Steam's game library/platform presentation](https://store.steampowered.com/about/) informed the library-selection/hero/action/updates hierarchy. No Steam artwork or UI assets are copied.
