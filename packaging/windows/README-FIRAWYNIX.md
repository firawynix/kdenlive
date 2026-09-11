# Distribuição do Firawynix - Kdenlive para Windows

O instalador usa Inno Setup e inclui um launcher que verifica a versão mais
recente publicada em `firawynix/kdenlive` antes de abrir o editor. O download
de atualização só é aceito quando o release contém estes dois arquivos:

- `Firawynix-Kdenlive-Setup-x64.exe`
- `Firawynix-Kdenlive-Setup-x64.exe.sha256`

O launcher compara a tag do último GitHub Release com sua versão incorporada,
baixa o instalador em `%LOCALAPPDATA%\Firawynix-Kdenlive\updates`, valida
SHA-256 e só então solicita a instalação. Sem rede, sem release ou sem hash, o
editor instalado abre normalmente.

Para produzir uma versão, execute `build-installer.ps1` após compilar o alvo
`kdenlive`. O script monta um pacote portátil com todas as dependências via KDE
Craft, substitui o executável pelo fork compilado, acrescenta o launcher e gera
o instalador e o arquivo de hash em `dist/windows`.

O projeto continua sob GPL e preserva os créditos do Kdenlive. O repositório
original é <https://github.com/KDE/kdenlive> e o fork Firawynix é
<https://github.com/firawynix/kdenlive>.
