const NL = window.Neutralino;

const $ = (id) => document.getElementById(id);

const els = {
  hostPath: $('hostPath'),
  btnHostBrowse: $('btnHostBrowse'),
  btnHostSave: $('btnHostSave'),
  btnInstall: $('btnInstall'),
  btnUninstall: $('btnUninstall'),
  hostStatus: $('hostStatus'),
  filePath: $('filePath'),
  wallName: $('wallName'),
  gameName: $('gameName'),
  authorName: $('authorName'),
  wallFormat: $('wallFormat'),
  btnBrowse: $('btnBrowse'),
  btnInject: $('btnInject'),
  btnRefresh: $('btnRefresh'),
  injectStatus: $('injectStatus'),
  wallList: $('wallList'),
};

function setStatus(text, kind) {
  els.injectStatus.textContent = text || '';
  els.injectStatus.className = 'status' + (kind ? ' ' + kind : '');
}

async function init() {
  try {
    NL.init();
  } catch (e) {
    setStatus('初始化失败：' + e.message, 'err');
    return;
  }
  refreshHostPath();
  refreshList();
}

function setHostStatus(text, kind) {
  els.hostStatus.textContent = text || '';
  els.hostStatus.className = 'status' + (kind ? ' ' + kind : '');
}

async function refreshHostPath() {
  try {
    const r = await runCli(['set-host']);
    const lines = r.stdOut.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
    const first = lines[0];
    if (r.exitCode === 0 && first && first !== 'not_set') {
      els.hostPath.value = first;
      setHostStatus('');
    } else {
      els.hostPath.value = '';
      setHostStatus('尚未设置，选择目录后点击保存（首次运行其他命令时会自动检测）');
    }
  } catch (e) {
    setHostStatus('读取失败：' + e.message, 'err');
  }
}

async function onHostBrowse() {
  try {
    const entries = await NL.os.showFolderDialog('选择 N0vaDesktop 安装目录');
    if (entries && entries.length > 0) {
      els.hostPath.value = entries[0];
      setHostStatus('');
    }
  } catch (e) {
    setHostStatus('选择目录失败：' + e.message, 'err');
  }
}

async function onHostSave() {
  const dir = els.hostPath.value.trim();
  if (!dir) {
    setHostStatus('请先选择目录', 'err');
    return;
  }
  try {
    const r = await runCli(['set-host', dir]);
    if (r.exitCode === 0) {
      els.hostPath.value = dir;
      setHostStatus('已保存', 'ok');
    } else {
      setHostStatus('保存失败：' + ((r.stdErr || r.stdOut || '').trim() || '目录无效'), 'err');
    }
  } catch (e) {
    setHostStatus('保存失败：' + e.message, 'err');
  }
}

async function runInstallCmd(cmd) {
  els.btnInstall.disabled = true;
  els.btnUninstall.disabled = true;
  try {
    const r = await runCli([cmd]);
    const text = (r.stdOut + '\n' + r.stdErr).trim();
    if (r.exitCode === 0) {
      setHostStatus(text || (cmd === 'install' ? '安装完成' : '卸载完成'), 'ok');
    } else {
      setHostStatus((cmd === 'install' ? '安装失败：\n' : '卸载失败：\n') + (text || '未知错误'), 'err');
    }
  } catch (e) {
    setHostStatus('执行失败：' + e.message, 'err');
  } finally {
    els.btnInstall.disabled = false;
    els.btnUninstall.disabled = false;
  }
}

async function onInstall() {
  setHostStatus('正在安装…（请确保人工桌面已关闭）');
  await runInstallCmd('install');
}

async function onUninstall() {
  setHostStatus('正在卸载…（请确保人工桌面已关闭）');
  await runInstallCmd('uninstall');
}

function quoteArg(s) {
  return '"' + s.replace(/"/g, '\\"') + '"';
}

function getCliPath() {
  const dir = (window.NL_PATH || '').replace(/[\\/]+$/, '');
  if (!dir) throw new Error('无法获取程序目录');
  return dir + '\\n0va_plugin.exe';
}

async function runCli(args) {
  const exe = getCliPath();
  const cmd = [quoteArg(exe)].concat(args.map(quoteArg)).join(' ');
  return await NL.os.execCommand(cmd);
}

function showSpinner(on) {
  els.btnInject.disabled = on;
  els.btnInject.textContent = on ? '导入中…' : '导入壁纸';
}

function fmtResult(r) {
  if (r.exitCode === 0) return { ok: true, text: r.stdOut.trim() };
  const text = (r.stdErr || r.stdOut || '未知错误').trim();
  if (/不是内部或外部命令|not recognized|not found/i.test(text)) {
    return { ok: false, text: '未找到 n0va_plugin.exe，请将它与本程序放在同一目录' };
  }
  return { ok: false, text: text };
}

async function onInject() {
  const file = els.filePath.value.trim();
  if (!file) {
    setStatus('请先选择壁纸文件', 'err');
    return;
  }
  const name = els.wallName.value.trim();
  const game = els.gameName.value.trim() || '原神';
  const author = els.authorName.value.trim();
  const format = els.wallFormat.value;

  const args = ['inject', file];
  if (name) args.push('--name', name);
  if (game) args.push('--game-name', game);
  if (author) args.push('--author', author);
  if (format === 'static') args.push('--format', 'static');
  else if (format === 'dynamic') args.push('--format', 'dynamic');

  showSpinner(true);
  setStatus('正在导入…（动态壁纸需生成预览图，可能稍慢）');
  try {
    const r = await runCli(args);
    const res = fmtResult(r);
    if (res.ok) {
      setStatus(res.text || '导入成功', 'ok');
      refreshList();
    } else {
      setStatus('导入失败：\n' + res.text, 'err');
    }
  } catch (e) {
    setStatus('执行失败：' + e.message, 'err');
  } finally {
    showSpinner(false);
  }
}

async function onBrowse() {
  try {
    const entries = await NL.os.showOpenDialog('选择壁纸文件', {
      filters: [
        { name: '壁纸文件', extensions: ['png', 'jpg', 'jpeg', 'mp4'] },
        { name: '所有文件', extensions: ['*'] },
      ],
    });
    if (entries && entries.length > 0) {
      els.filePath.value = entries[0];
      const m = entries[0].match(/([^\\/]+)\.[^.]+$/);
      if (m) els.wallName.value = m[1];
    }
  } catch (e) {
    setStatus('打开文件对话框失败：' + e.message, 'err');
  }
}

async function refreshList() {
  els.wallList.innerHTML = '<div class="empty">加载中…</div>';
  try {
    const r = await runCli(['list', '--json']);
    let items = [];
    if (r.exitCode === 0 && r.stdOut.trim()) {
      try {
        items = JSON.parse(r.stdOut);
      } catch (e) {
        items = [];
      }
    }
    renderList(items);
  } catch (e) {
    els.wallList.innerHTML = '<div class="empty">获取列表失败</div>';
  }
}

function renderList(items) {
  if (!items || items.length === 0) {
    els.wallList.innerHTML = '<div class="empty">还没有导入过壁纸</div>';
    return;
  }
  const rows = items.map((it) => {
    return (
      '<div class="wall-item">' +
        '<span class="w-name">' + escHtml(it.name || '(未命名)') + '</span>' +
      '</div>'
    );
  });
  els.wallList.innerHTML = rows.join('');
}

function escHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

els.btnBrowse.addEventListener('click', onBrowse);
els.btnInject.addEventListener('click', onInject);
els.btnRefresh.addEventListener('click', refreshList);
els.btnHostBrowse.addEventListener('click', onHostBrowse);
els.btnHostSave.addEventListener('click', onHostSave);
els.btnInstall.addEventListener('click', onInstall);
els.btnUninstall.addEventListener('click', onUninstall);

init();
