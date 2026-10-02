/* Rodapé compartilhado das páginas públicas dos projetos Firawynix. */
(() => {
  const script = document.currentScript;
  const project = script?.dataset.project;
  const projects = {
    firawssh: { name: 'Firaw SSH', support: 'firawssh', downloads: [
      ['Instalador online · Windows', 'https://ssh.firawynix.com.br/downloads/Firaw-SSH-Instalador-Online.exe'],
      ['Microsoft Store', 'https://apps.microsoft.com/detail/9PBH4PV7XHG8'],
    ] },
    firawselector: { name: 'FirawSelector', support: 'firawselector', github: 'https://github.com/firawynix/firaw-selector', downloads: [
      ['Instalador online · Windows', 'https://firawselector.firawynix.com.br/downloads/FirawSelector-Instalador-Online.exe'],
      ['AppImage · Linux', 'https://jogos.firawynix.com.br/api/games/firawselector/linux/arquivo'],
      ['Microsoft Store', 'https://apps.microsoft.com/detail/9N8MV05X1NVP'],
    ] },
    folderpin: { name: 'Firaw TaskBar', support: 'folderpin', github: 'https://github.com/firawynix/folderpin', downloads: [
      ['Instalador online · Windows', 'https://folderpin.firawynix.com.br/downloads/Firaw-TaskBar-Instalador-Online.exe'],
      ['AppImage · Linux', 'https://github.com/firawynix/folderpin/releases/latest/download/Firaw-TaskBar-1.3.1-x86_64.AppImage'],
      ['Microsoft Store', 'https://apps.microsoft.com/detail/9PCQ0GS2ZK1P'],
    ] },
    snapcopytext: { name: 'SnapCopyText', support: 'snapcopytext', github: 'https://github.com/firawynix/firaw-snapcopytext', downloads: [
      ['Instalador online · Windows', 'https://snapcopytext.firawynix.com.br/downloads/Firaw-SnapCopyText-Instalador-Online.exe'],
      ['AppImage · Linux', 'https://jogos.firawynix.com.br/api/games/snapcopytext/linux/arquivo'],
      ['Microsoft Store', 'https://apps.microsoft.com/detail/9MTGVTHWB89K'],
    ] },
    transcricao: { name: 'Transcrição OBS', support: 'transcricao', github: 'https://github.com/firawynix/obs-transcricao-releases', downloads: [
      ['Instalador online · Windows', 'https://transcricao.firawynix.com.br/downloads/Firaw-Transcricao-OBS-Instalador-Online.exe'],
      ['AppImage · Linux', 'https://jogos.firawynix.com.br/api/games/transcricao-obs/linux/arquivo'],
    ] },
    firawremote: { name: 'Firaw Remote', support: 'firawremote', github: 'https://github.com/firawynix/firaw-remote-public', downloads: [
      ['Instalador online · Windows', 'https://remote.firawynix.com.br/downloads/Firaw-Remote-Online.exe'],
    ] },
    kdenlive: { name: 'Firawynix Kdenlive', support: 'kdenlive', github: 'https://github.com/firawynix/kdenlive', downloads: [
      ['Instalador online · Windows', 'https://kdenlive.firawynix.com.br/downloads/Firawynix-Kdenlive-Instalador-Online.exe'],
    ] },
    series: { name: 'Minhas Séries', support: 'series', downloads: [
      ['Instalar no celular', 'https://series.firawynix.com.br/instalar'],
    ] },
    centrino: { name: 'Centrino DevOps', support: 'centrino' },
    curriculo: { name: 'Currículo em PDF', support: 'curriculo' },
    erp: { name: 'ERP + Caixa', support: 'erp' },
    erp_landing: { name: 'ERP + Caixa', support: 'erp' },
    loja: { name: 'Loja Online', support: 'loja' },
    servicedesk: { name: 'Service Desk', support: 'servicedesk' },
    firachat: { name: 'FiraChat', support: 'firachat' },
    tibia: { name: 'Tibia H4K', support: 'tibia', downloads: [
      ['Baixar cliente', 'https://global.h4k.com.br/?subtopic=downloadclient'],
    ] },
    baiak: { name: 'Baiak H4K', support: 'baiak', downloads: [
      ['Baixar cliente', 'https://baiak.h4k.com.br/?subtopic=downloadclient'],
    ] },
    aion: { name: 'Aion H4K', support: 'aion' },
    mu: { name: 'MU Online H4K', support: 'muonline' },
    minigolf: { name: 'Minigolf H4K', support: 'minigolf' },
  };
  const item = projects[project];
  if (!item || document.querySelector('.fw-footer')) return;

  const makeLink = (label, href, className = '') => {
    const link = document.createElement('a');
    link.textContent = label;
    link.href = href;
    link.className = className;
    if (/^https?:/.test(href) && new URL(href).host !== location.host) {
      link.target = '_blank';
      link.rel = 'noopener noreferrer';
    }
    return link;
  };

  const render = () => {
    const footer = document.createElement('footer');
    footer.className = 'fw-footer';
    footer.setAttribute('aria-label', `Links de ${item.name}`);
    const inner = document.createElement('div');
    inner.className = 'fw-footer-inner';
    const brand = document.createElement('a');
    brand.className = 'fw-footer-brand';
    brand.href = '#';
    brand.textContent = item.name;
    inner.append(brand);
    const credit = document.createElement('span');
    credit.className = 'fw-footer-credit';
    credit.textContent = 'Feito por Firawynix · 2026';
    inner.append(credit);
    const nav = document.createElement('nav');
    nav.className = 'fw-footer-links';
    nav.setAttribute('aria-label', 'Links do projeto');
    if (item.downloads?.length) {
      const details = document.createElement('details');
      details.className = 'fw-footer-downloads';
      const summary = document.createElement('summary');
      summary.textContent = '↓ Downloads';
      const menu = document.createElement('div');
      menu.className = 'fw-footer-download-menu';
      for (const [label, href] of item.downloads) menu.append(makeLink(label, href));
      details.append(summary, menu);
      nav.append(details);
    }
    nav.append(makeLink('GitHub ↗', item.github || 'https://github.com/firawynix'));
    nav.append(makeLink('Portfólio ↗', 'https://firawynix.com.br/'));
    nav.append(makeLink('♥ Apoiar', `https://firawynix.com.br/apoie?de=${encodeURIComponent(item.support)}`, 'fw-footer-support'));
    inner.append(nav);
    footer.append(inner);
    document.body.classList.add('fw-footer-active');
    document.body.append(footer);
    // Os links do dock antigo foram migrados para a barra compartilhada.
    document.querySelectorAll('footer.dock').forEach((old) => {
      old.style.setProperty('display', 'none', 'important');
    });
  };
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', render, { once: true });
  else render();
})();
