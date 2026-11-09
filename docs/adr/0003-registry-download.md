# ADR 0003 (desktop) — Download do Registry por mirrors e Registry embutido

**Status:** Aceita · **Data:** 2026-11-09 · **Implementa:** tarefa D1.4 do roadmap e a seção 8 da spec do Registry.

## Contexto
O app só detecta e faz backup de jogos para os quais tem perfis. Até aqui, os perfis só chegavam por importação manual de uma pasta, e quem instala o app começava sem perfil nenhum. A spec do Registry (`saveforge-specs/registry/SPEC.md`) define arquivos estáticos assinados, servidos por uma lista de mirrors intercambiáveis, e exige que uma falha de rede nunca reduza os perfis disponíveis.

## Decisão
- **Uma só verificação.** O download não verifica nada por conta própria. Ele baixa os arquivos de um mirror para uma pasta temporária dentro do cache (`.staging-*`), no mesmo layout de uma distribuição, e entrega essa pasta ao `RegistryCache::importChannel()`. Essa é a mesma verificação da importação manual e do cache local: cadeia de root, assinatura do índice, anti-rollback, hash e tamanho do bundle.
- **Decisões antes do bundle.** O índice é lido antes de ser verificado só para decidir o que baixar. Se a sequência for igual à do cache, nada é baixado. Se for menor, o mirror é recusado como rollback sem baixar o bundle. Como tudo é verificado antes de o cache mudar, um índice adulterado no máximo impede uma atualização, o mesmo efeito de estar sem rede.
- **Mirrors.** Os mirrors são tentados em ordem, e cada canal fica com o primeiro que verificar. São aceitos URLs HTTPS, HTTP apenas para loopback (testes e servidores locais) e pastas locais. Mesmo com o conteúdo assinado, HTTP comum expõe na rede quais perfis o usuário baixa. As requisições levam só um `User-Agent` com a versão do app.
- **Limites.** Arquivos de metadados podem ter até 1 MiB, e o bundle até o tamanho declarado no índice, sem passar de 64 MiB. Assim, um mirror hostil não consegue esgotar a memória antes da verificação.
- **Configuração no build.** `SAVEFORGE_REGISTRY_MIRRORS` define os mirrors padrão. `SAVEFORGE_REGISTRY_BUNDLED_DIR` embute uma distribuição como recurso Qt, que alimenta o cache na primeira execução, só para os canais que ele ainda não tem. Por padrão os dois ficam vazios, porque ainda não existem chaves de produção nem um endereço oficial de publicação.
- **No app.** A atualização automática roda no máximo uma vez por dia, depois da primeira varredura, e pode ser desligada nas configurações. Falhas de rede na verificação automática não interrompem o usuário. Dados que não passam na verificação sempre são avisados.

## Consequências
- (+) Download, importação manual e Registry embutido passam pelo mesmo código de verificação, já testado com os vetores das specs.
- (+) O app funciona offline desde a primeira execução quando o build embute um Registry.
- (+) Testes de HTTP rodam sem rede, com um servidor local (`tests/support/StaticHttpServer.h`).
- (−) O módulo `registry` passa a depender do QtNetwork.
- (−) Ainda não há `ETag`/`If-None-Match`. A comparação de sequência já evita baixar o bundle de novo, e o índice tem poucos KB.

## Alternativas
- **Verificar em memória, sem pasta temporária:** evitaria escrever em disco, mas exigiria uma segunda implementação da verificação, que hoje lê de uma pasta.
- **Baixar a distribuição inteira (todos os canais) de uma vez:** mais simples, mas baixaria canais desativados, como `community-experimental`.
