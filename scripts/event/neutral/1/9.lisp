
(dialog "An old drone carrier approaches and activates. Looks like it hasn't met anyone in a long time...")


(opponent-init 7 'neutral)

(island-configure
 (opponent)
 '((hull 0 12)
   (hull 0 13)
   (hull 0 14)
   (hull 0 11)
   (hull 1 14)
   (power-core 1 12)
   (hull 1 11)
   (hull 2 14)
   (hull 2 11)
   (masonry 3 12 0)
   (hull 3 14)
   (drone-bay 3 13)
   (masonry 3 10 0)
   (drone-bay 3 11)
   (masonry 4 12 0)
   (hull 4 14)
   (masonry 4 10 0)
   (masonry 5 12 0)
   (drone-bay 5 13)
   (hull 5 14)
   (drone-bay 5 11)
   (masonry 5 10 0)
   (masonry 6 12 0)
   (hull 6 14)
   (masonry 6 10 0)))


(secret 6 14 "yes no")


(defn on-converge
  ;; want drones?
  (dialog "<c:robot:12> 01010111 01100001 01101110 01110100 00100000 01100100 01110010 01101111 01101110 01100101 01110011 00111111?")
  (dialog-await-y/n)

  (defn on-dialog-accepted
    ;; less than 2?
    (dialog "<c:robot:12> 00111100 00100000 00110010?")
    (dialog-await-y/n)

    (defn on-dialog-accepted
      ;; place one drone bay
      (while (not (construction-sites (player) '(2 . 1)))
        (terrain (player) (+ terrain (player)) 1))

      (sel-input '(2 . 1)
                 "Pick a slot (2x1)"
                 (lambda
                   (syscall "sound" "build0")
                   (room-new (player) `(drone-bay ,$1 ,$2))
                   (dialog "<c:robot:12> 01000010 01111001 01100101!")
                   (exit))))

    (defn on-dialog-declined
      ;; place two drone bays
      (while (not (construction-sites (player) '(2 . 1)))
        (terrain (player) (+ terrain (player)) 1))

      (sel-input '(2 . 1)
                 "Pick a slot (2x1) (1 of 2)"
                 (lambda
                   (syscall "sound" "build0")
                   (room-new (player) `(drone-bay ,$1 ,$2))

                   (sel-input '(2 . 1)
                              "Pick a slot (2x1) (2 of 2)"
                              (lambda
                                (syscall "sound" "build0")
                                (room-new (player) `(drone-bay ,$1 ,$2))
                                (dialog "<c:robot:12> 01000010 01111001 01100101!")
                                (exit)))))))

  (setq on-dialog-declined exit))