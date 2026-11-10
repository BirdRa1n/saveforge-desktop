#pragma once

#include <QString>

namespace savevault {

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
    Untrusted,      // signature, trust chain or anti-rollback check failed
    Network,        // a remote source could not be reached or answered with an error
    TooLarge,       // more data than expected or allowed (e.g. a profile's maxBytes)
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

} // namespace savevault
