compile: 5stage 5stage_bypass

5stage: 5stage.cpp
	g++ -std=c++11 5stage.cpp -o 5stage

5stage_bypass :5stage_bypass.cpp
	g++ -std=c++11 5stage_bypass.cpp -o 5stage_bypass

run_5stage: 5stage
	./5stage input.asm

run_5stage_bypass: 5stage_bypass
	./5stage_bypass input.asm

clean:
	rm 5stage
	rm 5stage_bypass

