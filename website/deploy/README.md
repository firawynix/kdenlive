# Publicação no servidor Firawynix

O site estático é executado no servidor `10.81.66.10`, na porta privada `26004`.
O Cloudflare Tunnel existente no servidor `10.81.66.7` publica essa porta em
`kdenlive.firawynix.com.br`.

## A fonte de verdade é o 10.81.66.7

Os arquivos ficam em `/opt/firawynix-kdenlive-site/` no **10.81.66.7**. O
`sync-standby` (a cada 15 min) espelha o `/opt/` inteiro para o `10.81.66.10` com
`rsync --delete`: pasta criada só no `.10` é apagada no ciclo seguinte — foi o que
derrubou a primeira publicação (o container ficou servindo uma pasta vazia, 403).

Atualizar o conteúdo não exige reconstruir a imagem:

1. copiar `dist/index.html` e `dist/assets/` para `/opt/firawynix-kdenlive-site/public/`
   no `.7`;
2. `sudo systemctl start sync-standby.service` no `.7` (ou esperar o timer);
3. conferir `curl http://10.81.66.10:26004/` a partir do `.7`.

O `nginx.conf` é montado como arquivo: o rsync troca o arquivo inteiro, e o
container continua lendo o antigo. Mudou o `nginx.conf`, rode
`docker compose up -d --force-recreate` no `.10`. O mesmo vale se a pasta
`/opt/firawynix-kdenlive-site` for recriada.
