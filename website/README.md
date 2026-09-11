# Site demonstrativo

Página estática e responsiva do Firawynix - Kdenlive, inspirada na experiência
guiada do FirawSelector. Abra `index.html` ou sirva a pasta `website` em qualquer
servidor HTTP estático.

Produção: <https://kdenlive.firawynix.com.br>. O container roda no servidor
Firawynix `10.81.66.10` e é publicado pelo Cloudflare Tunnel existente, mas os
arquivos são copiados para o **`10.81.66.7`** (`/opt/firawynix-kdenlive-site/`),
de onde a sincronização do espelho os leva ao `.10`. `dist/index.html` é cópia
exata de `index.html`. Consulte `deploy/README.md` para o passo a passo.

O rodapé fica preso ao pé da tela (como o do FirawSelector) com baixar o
instalador (`releases/latest/download/...`, sempre a versão mais recente), os dois
repositórios e o botão Apoiar (`firawynix.com.br/apoie?de=kdenlive`).

Os links de autoria apontam tanto para o projeto original
<https://github.com/KDE/kdenlive> quanto para o fork
<https://github.com/firawynix/kdenlive>.
