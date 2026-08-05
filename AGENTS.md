# linux-fullscreen — leia o mapa antes de agir

Este arquivo existe para agentes que procuram `AGENTS.md` (o `agy`, entre
outros). Ele é curto de propósito: **o mapa do projeto é o `CLAUDE.md`**, e ele
não é copiado para cá.

**Antes de responder qualquer coisa sobre este projeto, leia o `CLAUDE.md` da
raiz.** Ele traz a arquitetura, a tabela de arquivos, as invariantes que quebram
o ambiente se violadas, e as armadilhas que já custaram horas. Sem ele você vai
sugerir coisas que este projeto já mediu e recusou.

O `README.md` tem ~4400 linhas e é o caderno de laboratório. **Não leia
inteiro**: use `grep -n '^## \|^### ' README.md` para o índice e leia só a seção
necessária.

## Por que um ponteiro, e não uma cópia

Duplicar o `CLAUDE.md` aqui daria duas fontes da verdade, e a segunda envelhece
calada. Um symlink também não serve: medido em 05/08/2026, o `agy` **não carrega
`AGENTS.md` quando ele é symlink** — a conversa subiu sem o mapa e sem erro
nenhum na tela (só uma linha de `rules.go` no log). Arquivo de verdade com um
ponteiro é o que funciona e o que não sai de sincronia.
