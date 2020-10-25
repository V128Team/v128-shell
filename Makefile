all: deb

deb:
	dpkg-buildpackage -tc --no-sign

clean:
	rm -rf debian/v128-shell/ dist/ build/ v128-shell.egg-info/ __pycache__/

.PHONY: all deb clean
