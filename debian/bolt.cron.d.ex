#
# Regular cron jobs for the bolt package
#
0 4	* * *	root	[ -x /usr/bin/bolt_maintenance ] && /usr/bin/bolt_maintenance
