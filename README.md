# Trabalho Prático 2 - Restaurante

## Departamento de Electrónica, Telecomunicações e Informática da Universidade de Aveiro
Curso: Licenciatura em Engenharia Informática  
Cadeira: Sistemas Operativos  
Ano Letivo: 2023/2024 — 1º ano, 1º Semestre  
`Nota: 20.0`

## Objetivo

Compreensão dos mecanismos associados à execução e sincronização de processos e threads.

## Guião

Vários grupos devem jantar num restaurante que possui duas mesas, um recepcionista, um empregado de mesa e um cozinheiro. 
Cada grupo deve seguir um procedimento específico, interagindo com os funcionários do restaurante. O objetivo é simular 
o funcionamento do restaurante utilizando processos independentes, sincronizados através de semáforos e memória partilhada.

Tomando como ponto de partida o código fonte disponível na página da disciplina, que contém documentação interna, desenvolva 
uma aplicação em C que simule o restaurante. Os grupos, recepcionista, empregado e cozinheiro serão processos independentes. 
A sincronização deve ser realizada através de semáforos e memória partilhada. Todos os processos são criados no início do programa 
e estão em execução a partir dessa altura. Os processos devem estar ativos apenas quando necessário, bloqueando sempre que 
tiverem que esperar por algum evento. No final, todos os processos devem terminar.

Para visualizar o resultado da execução de todos os processos numa versão pré-compilada, execute o seguinte comando:
```bash
cd src
make all_bin
cd run
./probSemSharedMemRestaurant
```

Apenas devem ser alterados os ficheiros `semSharedMemGroup.c`, `semSharedMemChef.c`, `semSharedMemWaiter.c` e `semSharedMemReceptionist.c`. 
Dentro destes ficheiros estão assinaladas as zonas a alterar.

O trabalho será realizado em grupos de 2 alunos, respeitando um código de ética rigoroso que proíbe o plágio e a execução do trabalho por 
elementos externos ao grupo. A entrega do trabalho será realizada através do elearning.ua.pt e deverá incluir o código fonte da solução 
e um relatório descrevendo a abordagem usada para resolver o problema e os testes realizados para validar a solução.
