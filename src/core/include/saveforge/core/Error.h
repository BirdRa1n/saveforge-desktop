#pragma once

#include <QString>

namespace saveforge {

// Categorias estáveis de erro. Novas categorias são adicionadas ao final.
enum class ErrorCode {
    Unknown,
    InvalidArgument,
    NotFound,
    Io,
    Corrupted,
    Unsupported,
    PermissionDenied,
    Cancelled,
};

// Erro de domínio: código estável + mensagem legível + contexto opcional
// (ex.: caminho do arquivo). Nunca atravessa fronteiras de módulo como exceção.
struct Error {
    ErrorCode code = ErrorCode::Unknown;
    QString message;
    QString context;

    [[nodiscard]] QString toString() const;
};

[[nodiscard]] QString errorCodeName(ErrorCode code);

} // namespace saveforge
