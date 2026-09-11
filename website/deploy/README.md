# Publicação no servidor Firawynix

O site estático é executado no servidor `10.81.66.10`, na porta privada `26004`.
O Cloudflare Tunnel existente no servidor `10.81.66.7` publica essa porta em
`kdenlive.firawynix.com.br`.

Os arquivos publicados ficam em `public/` no servidor. A atualização do conteúdo
não exige reconstruir a imagem; basta substituir os arquivos estáticos.

