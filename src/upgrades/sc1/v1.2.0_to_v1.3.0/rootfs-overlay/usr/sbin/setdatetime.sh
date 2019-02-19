#!/bin/sh

# Example of date format from the wget:
#   13 Oct 2018 00:15:11
CURR_DATE=`wget -qSO- -T 5 google.com 2>&1 | grep Date: | cut -d' ' -f5-8`
CURR_DAY=`echo "$CURR_DATE" | awk '{print $1}'`
CURR_MONTH_STR=`echo "$CURR_DATE" | awk '{print $2}'`
CURR_YEAR=`echo "$CURR_DATE" | awk '{print $3}'`
CURR_TIME=`echo "$CURR_DATE" | awk '{print $4}'`

# Convert string month to a number, as 'date -s' requires a specific format
case $CURR_MONTH_STR in
  "Jan") CURR_MONTH_NUM="1" ;;
  "Feb") CURR_MONTH_NUM="2" ;;
  "Mar") CURR_MONTH_NUM="3" ;;
  "Apr") CURR_MONTH_NUM="4" ;;
  "May") CURR_MONTH_NUM="5" ;;
  "Jun") CURR_MONTH_NUM="6" ;;
  "Jul") CURR_MONTH_NUM="7" ;;
  "Aug") CURR_MONTH_NUM="8" ;;
  "Sep") CURR_MONTH_NUM="9" ;;
  "Oct") CURR_MONTH_NUM="10" ;;
  "Nov") CURR_MONTH_NUM="11" ;;
  "Dec") CURR_MONTH_NUM="12" ;;
   *) CURR_MONTH_NUM=1 ;;
esac

# Small sanity check to make sure that date is reasonable
if [ $CURR_YEAR -gt 2017 ]
then
  date --utc -s "$CURR_YEAR-$CURR_MONTH_NUM-$CURR_DAY $CURR_TIME"
fi