# ADR 0008 (desktop) — Consumo de memória e custo de inicialização

**Status:** Aceita · **Data:** 2026-11-10

## Contexto
O SaveVault passa a maior parte do tempo na bandeja, esperando para fazer os backups automáticos. Na 0.3.0-alpha.2, medido no Windows com os 22 mil perfis do registry e 30 jogos, ele usava:

- ~171 MB de memória privada na bandeja, o mesmo valor que com a janela aberta;
- ~300 MB de pico na abertura;
- ~7,8 s de CPU a cada inicialização.

As medições isolaram de onde vinha esse custo:

- **A interface (Qt Quick, Direct3D 11, QML e imagens) custava ~120 MB e continuava carregada com a janela escondida.** O Direct3D 11, que é o padrão do Qt no Windows, foi o backend mais leve. D3D12 e OpenGL custaram 90 a 130 MB a mais.
- **`QJsonDocument` montava a árvore do bundle inteiro, ~107 MB para 12,5 MB de texto,** só para entregar os perfis um a um.
- **Na varredura, o resolvedor de caminhos listava as mesmas pastas uma vez por perfil.** Em Windows, que não diferencia maiúsculas, cada segmento existente gerava uma listagem. Isso consumia ~4,5 s.
- **A validação dos perfis compilava expressões regulares a cada comparação de `*`/`?` e a cada caminho.**
- **O catálogo inteiro voltava para a thread da interface só para ser contado,** e a contagem recalculava a precedência dos canais.

## Decisão
- **Interface descarregada na bandeja:**
  - `UiHost` (app) cria o `QQmlApplicationEngine` sob demanda.
  - Cinco segundos depois de a janela ser escondida, ele destrói o engine. Ao reabrir, cria de novo, restaurando a geometria e o jogo selecionado, sem nova varredura.
  - Para o controlador sobreviver, o executável cria o `AppController` e o registra com `AppController::setSharedInstance`. A fábrica `create()` do singleton QML devolve essa instância com `CppOwnership`.
  - O construtor não tem argumento padrão, porque o Qt 6.12 prefere um construtor padrão a `create()`.
- **Bundle lido por partes:**
  - `registry::JsonSpans` localiza os valores no texto sem montar um documento.
  - Cada perfil é lido com `QJsonDocument` isoladamente, ainda validado pelo parser do Qt. O bundle já foi verificado por assinatura e hash antes disso.
- **Varredura com `ProbeCache`:** `PathResolver::resolve(..., ProbeCache&)` lista cada pasta uma vez por passada. `detectGames` usa um cache por varredura. O uso fora da varredura não muda.
- **Validação sem regex nos caminhos quentes:**
  - `wildcardMatch` (interno ao módulo `profiles`) compara `*`/`?` sem alocar.
  - As regras P1–P3 e `impliedOs` usam varreduras de caracteres equivalentes às expressões regulares que substituem.
- **Catálogo fica na thread de trabalho:** `RefreshResult` leva à interface só a contagem, os avisos e os jogos encontrados. `scanLibrary` ganhou uma sobrecarga que recebe as entradas já resolvidas.

## Consequências
Medido no mesmo cenário:

| | Antes | Depois |
|---|---|---|
| Bandeja | 171 MB | **~56 MB** |
| Janela aberta | ~170 MB | ~155 MB |
| Pico na abertura | ~300 MB | ~210 MB |
| CPU na abertura | ~7,8 s | ~2,6 s |

- (+) A varredura detecta exatamente os mesmos jogos. A saída do `savevault-cli detect` foi comparada linha a linha.
- (−) Reabrir a janela pela bandeja recria a interface. A biblioteca não é varrida de novo, mas há uma pausa curta.
- (−) Um `ProbeCache` não vê arquivos criados depois da listagem. Por isso ele vale só para uma passada curta.
- (−) Cerca de 20 MB da memória usada pela leitura dos perfis continuam reservados pelo alocador depois de liberados (fragmentação do heap). Uma varredura em processo separado eliminaria isso, a um custo de complexidade que não compensa hoje.
