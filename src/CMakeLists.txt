# Cada módulo é uma biblioteca estática com dependência unidirecional:
#   app/cli -> services -> (detection, vault, archive, registry, db) -> profiles -> platform -> core
# Os módulos são adicionados aqui à medida que forem implementados (ver roadmap).
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(profiles)
add_subdirectory(app)
add_subdirectory(cli)
