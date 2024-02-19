arch = $(shell uname -m)
sdriq:
	pip install -e .

clean:
	rm -r build *.so sdrplay.egg-info
