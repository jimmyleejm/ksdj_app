
SUBDIRS=C

LINPHONE_SOUNDS=ringback.wav 
LINPHONE_RINGS=	rings/oldphone.wav \
		rings/rock.wav \
		rings/bigben.wav \
		rings/toy-mono.wav

sounddir=$(datadir)/sounds/linphone

sound_DATA=$(LINPHONE_SOUNDS)

ringdir=$(datadir)/sounds/linphone/rings

ring_DATA=$(LINPHONE_RINGS)

#to be compliant with freedesktop.org:
linphone_fddir= $(datadir)/applications
linphone_fd_DATA= linphone.desktop


pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=linphone.pc

linphonedir=$(datadir)/linphone
linphone_DATA=rootca.pem
rootca.pem: 
	$(top_srcdir)/scripts/mk-ca-bundle.pl rootca.pem

EXTRA_DIST = 					$(LINPHONE_SOUNDS) \
						$(LINPHONE_RINGS) \
					        linphone.desktop.in   \
						linphone.pc.in \
						rootca.pem

