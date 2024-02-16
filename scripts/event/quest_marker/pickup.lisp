

(lc-dialog-load "taxi-quest" "intro")


(opponent-init 7 'neutral)


(island-configure
 (opponent)
 '((power-core 5 13)))

(chr-new (opponent) 1 14 'neutral 0)
(chr-new (opponent) 2 14 'neutral 0)
(chr-new (opponent) 3 14 'neutral 0)


(defn on-dialog-closed [0]
  (setq on-dialog-closed exit)
  (map
   (lambda
     (let ((slot (chr-slots (player))))
       (if (not slot)
           (let ((s (construction-sites (player) '(1 . 2))))
             (if s
                 (progn
                   (room-new (player) (list 'ladder (caar s) (cdr (car s))))
                   (setq slot (chr-slots (player)))))))
       (if slot
           (progn
             (chr-new (player)
                      (caar slot)
                      (cdr (car slot))
                      'neutral
                      nil)
             (chr-del (opponent) (get $0 0) (get $0 1))))))
   '((1 14 10) (2 14 16) (3 14 5)))

  (adventure-log-add 25 '())

  (if (equal 0 (length (chrs (opponent))))
      (lc-dialog-load "taxi-quest" "join1")
    (if (< 3 (length (chrs (opponent))))
        (lc-dialog-load "taxi-quest" "join2")
      (lc-dialog-load "taxi-quest" "join3"))))


(defn on-converge [0]
  (lc-dialog-load "taxi-quest" "greeting")
  (coins-add 1500))
