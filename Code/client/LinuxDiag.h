#pragma once

// Diagnóstico temporário do port Linux: log síncrono (WriteFile + FlushFileBuffers,
// fsync real) que sobrevive a um crash imediato do processo, ao contrário do
// spdlog em buffer. Usado para isolar o crash 0x80000003 que ocorre no loop de
// jogo sob Proton. Remover quando resolvido.
void LinuxDiagStep(const char* apStep);
