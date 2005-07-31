#!/bin/bash
#
# get rid of all ocha configuration on gconf
#
gconftool-2 --recursive-unset /apps/ocha 
rm ~/.ocha/catalog