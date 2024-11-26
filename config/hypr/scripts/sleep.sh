hypridle -w timeout 300 'hyprlock -f -c 000000' \
            timeout 600 'systemctl suspend' \
            before-sleep 'hyprlock -f -c 000000' &
