# Script to generate plots for every input combination
import os

Ki = [1, 4]
Km = [1, 1.5]
Kn = [0.5, 1]
Kf = [0.1, 0.3]
Ps = [0.99, 0.9999]
T = 1000

count = 1
for ki in Ki:
	for km in Km:
		for kn in Kn:
			for kf in Kf:
				for ps in Ps:
					os.system(f"python3 CS19B005.py -i {ki} -m {km} -n {kn} -f {kf} -s {ps} -T {T} -o test{count}")
					count += 1