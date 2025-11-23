CC = gcc
FLAGS = -O3 -DNDEBUG 
TARGET_BENCH    = benchmark
all:  $(TARGET_BENCH) 


$(TARGET_BENCH): test/benchmark.c slog.h
	$(CC) $(FLAGS) $< -o $(TARGET_BENCH)



run_bench: $(TARGET_BENCH)
	./$(TARGET_BENCH) 



clean:
	rm -f  $(TARGET_BENCH) 
