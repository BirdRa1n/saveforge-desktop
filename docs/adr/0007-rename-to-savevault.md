# ADR 0007 (desktop) — O projeto passa a se chamar SaveVault

**Status:** Aceita · **Data:** 2026-11-10

## Contexto
O mantenedor decidiu trocar o nome do projeto de SaveForge para SaveVault, que descreve melhor o que o app faz: guardar saves num cofre local. Já existe outro gerenciador de saves de código aberto com o nome SaveVault. Mesmo assim, o mantenedor preferiu manter a escolha.

A versão 0.2 já foi publicada com o nome antigo. Por isso, quem atualizar não pode perder dados, e os arquivos `.gamesave` exportados antes precisam continuar abrindo.

## Decisão
- **Renomeado em tudo:**
  - código: namespaces `savevault::`, cabeçalhos em `include/savevault/`, alvos CMake, módulos QML `SaveVault.Ui` e `SaveVault.Backend`;
  - executáveis `SaveVault` e `savevault-cli`;
  - variáveis de ambiente `SAVEVAULT_*`;
  - pacotes, ícone e documentação.
- **Pasta de dados:**
  - Passa a ser `…/SaveVault`.
  - Na primeira execução, `platform::appDataDirectory()` renomeia a pasta antiga (`…/SaveForge`), que guarda configurações, vault padrão, cache do registry e perfis locais.
  - Se a renomeação falhar (por exemplo, uma versão antiga ainda aberta), o app continua usando a pasta antiga.
- **Formatos (specs):**

  | O quê | Escrita | Leitura também aceita |
  |---|---|---|
  | `.gamesave`: media type | `application/vnd.savevault.gamesave+zip` | `application/vnd.saveforge.gamesave+zip` |
  | `.gamesave`: `format` do manifest | `savevault.gamesave` | `saveforge.gamesave` |
  | Perfis: `source` | `savevault` | `saveforge` (vira `savevault`) |
  | Configuração `restore/cloudPolicy` | `prefer-savevault` | `prefer-saveforge` |

- **Instalador do Windows:** o `AppId` continua o mesmo. Assim, o instalador novo atualiza a instalação antiga em vez de criar uma segunda.
- **Chaves de teste do registry:** continuam derivadas do rótulo `saveforge-test-key`. Trocar esse rótulo mudaria todas as chaves, assinaturas e vetores sem nenhum ganho.
- **Repositórios no GitHub:** passam a se chamar `savevault-*`. O mirror padrão do registry passa a ser `https://birdra1n.github.io/savevault-registry/v1/`.

## Consequências
- (+) Atualizar não perde configurações nem backups, e os `.gamesave` antigos continuam sendo importados.
- (−) Os leitores carregam alguns valores antigos para sempre. Eles ficam isolados e comentados nos pontos de leitura.
- (−) O GitHub redireciona os endereços dos repositórios renomeados, mas não os do GitHub Pages. Até o registry ser publicado no endereço novo, o mirror padrão fica indisponível, e o app usa o registry embutido.
