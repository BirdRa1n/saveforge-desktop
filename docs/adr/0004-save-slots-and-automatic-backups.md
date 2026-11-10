# ADR 0004 (desktop) — Saves com nome e backups automáticos

**Status:** Aceita · **Data:** 2026-11-10 · **Implementa:** a tarefa D2.2 do roadmap e o campo `slot` da spec do vault (`saveforge-specs` 469fd40).

## Contexto
Usuários querem manter progressos separados de um mesmo jogo, como outro personagem ou uma partida de teste, e alternar entre eles. O histórico de backups já guarda cada estado, mas não diz a que progresso cada backup pertence. Além disso, um app de backup só protege de verdade quando o usuário não precisa lembrar de clicar.

## Decisão

### Saves com nome
- Um save com nome é uma **linha de progresso**: os snapshots com o mesmo `slot` (um campo opcional, adicionado à spec do vault). O estado atual de um save é o seu snapshot mais novo, de qualquer tipo.
- O app guarda **qual save está no disco** por jogo, no `settings.ini`. Isso é estado local da máquina e não vai para o vault, que pode ser compartilhado.
- Todo backup, manual ou automático, recebe o save em uso. O usuário não precisa "salvar no save" à parte.
- **Trocar de save é restaurar** o snapshot mais novo do save escolhido. O snapshot `pre-restore` da restauração recebe o save que está sendo deixado, então ele vira o estado atual desse save. A troca reaproveita o plano de restauração e todas as proteções da spec: jogo fechado, cloud do launcher, desfazer e troca atômica. Também não cria um segundo snapshot.
- Restaurar qualquer snapshot do histórico torna o save desse snapshot o save em uso.
- Saves não são renomeados nem apagados por enquanto. A spec só permite mudar `label` e `pinned` num snapshot, e apagar fica para a retenção (D2.3).

### Backups automáticos
- **Ao fechar o jogo:** a cada 15 s, o app compara os processos em execução com os jogos da biblioteca, por nome de processo ou por executável dentro da pasta de instalação. Um jogo que estava aberto e fechou entra na fila (`trigger: game-exit`).
- **Periódico:** opcional, a cada 1, 6 ou 24 horas (`trigger: schedule`). Jogos abertos ficam para quando fecharem.
- **Sem backups repetidos:** antes de cada backup automático, o app compara caminhos, tamanhos e datas dos arquivos com o último snapshot, sem calcular hashes. Se nada mudou, não cria snapshot.
- Os backups automáticos usam a mesma fila de operações do app (`run()`), então nunca correm em paralelo com uma restauração.

### Bandeja do sistema
- O executável usa `QApplication` e `QSystemTrayIcon` (Qt Widgets). O módulo Qt Labs Platform só tem bandeja nativa no Linux e no macOS e, no Windows, cai no Qt Widgets de qualquer forma.
- Fechar a janela só a esconde quando há bandeja e o usuário permite ("Continuar na bandeja", ligado por padrão). Assim os backups automáticos continuam. Sair fica no menu do ícone.
- Notificações da bandeja aparecem apenas com a janela escondida: na primeira vez que ela é escondida e ao fim de cada backup automático.

## Consequências
- (+) Os saves com nome cabem no formato do vault sem tipos novos de snapshot, e funcionam em qualquer app que leia o campo `slot`.
- (+) Os backups automáticos não enchem o histórico de cópias iguais.
- (−) Versões anteriores do app leem o vault de forma estrita e recusam snapshots com `slot`. Isso é aceitável enquanto o formato está em rascunho (alpha).
- (−) O pacote passa a incluir o Qt Widgets (cerca de 5 MB).
- (−) A detecção de jogo aberto depende do perfil (nomes de processo) ou de uma instalação detectada. Jogos achados só pelos saves não disparam backup ao fechar; o backup periódico cobre esses casos.

## Alternativas
- **Usar `label` como nome do save:** dispensaria a mudança na spec, mas misturaria nomes de backups com nomes de progresso, e `label` pode ser editado.
- **Snapshot extra ao trocar de save, antes da restauração:** duplicaria o `pre-restore` que a restauração já cria.
- **Observar arquivos (`file-change`):** reage a cada gravação do jogo e gera muito ruído. Fica para depois, se for útil.
