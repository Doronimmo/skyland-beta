;;;
;;;


(dialog "An abandoned warship appears! No crew seems to be aboard; ship appears to be completely dormant...")


(opponent-init 11 'neutral)

(island-configure
 (opponent)
 '((hull 1 12 59)
   (hull 1 14 59)
   (hull 2 12 59)
   (forcefield* 0 12 20)
   (forcefield* 1 13 20)
   (incinerator 2 10)
   (beam-gun 2 13)
   (arc-gun 2 7)
   (bronze-hull 2 9)
   (arc-gun 2 8)
   (hull 2 6)
   (bronze-hull 2 14)
   (bronze-hull 3 14)
   (splitter 3 8)
   (bronze-hull 3 12)
   (masonry 4 14 3)
   (bronze-hull 4 10)
   (bronze-hull 4 12)
   (bronze-hull 4 11)
   (masonry 5 14 3)
   (masonry 5 7 3)
   (shrubbery 5 5)
   (masonry 5 8 3)
   (stairwell 5 9)
   (stacked-hull 5 6)
   (stairwell 6 11)
   (reactor 6 8)
   (missile-silo 6 6)
   (stacked-hull 7 6)
   (war-engine 7 11)
   (hull 7 7)
   (drone-bay 8 10)
   (rocket-bomb 8 6)
   (plundered-room 9 5)
   (drone-bay 9 8)
   (hull 10 14)
   (hull 10 13)
   (hull 10 11)
   (plundered-room 10 9)
   (windmill 10 12)))



(flag-show (opponent) 4)


(defn on-converge [0]
  (setq on-converge nil)
  (dialog
   "The ship's weapons seem to be more-or-less intact! Attempt to remove one?")

  (dialog-opts-reset)
  (apply dialog-opts-push (take 'beam-gun))
  (apply dialog-opts-push (take 'incinerator))
  (apply dialog-opts-push (take 'splitter))
  (dialog-opts-push "nope" (lambda
                             (unbind 'take)
                             (exit))))


(defn take [1]
  (let ((wpn $0))
    (list
     (string "take " (rinfo 'name wpn) "…")
     (lambda
       (sel-input
        wpn
        "Place where?"
        (lambda
          (room-new (player) `(,wpn ,$1 ,$2))

          (map (lambda
                 (when (equal (car $0) wpn)
                   (room-del (opponent) (get $0 1) (get $0 2))))
               (rooms (opponent)))


          (sound "build0")
          (dialog "The fortress remains quiet.<B:0> Now to remove the next one...")

          (let ((opts '(beam-gun incinerator splitter))
                (wake
                 (lambda
                   (opponent-mode 'hostile)
                   (dialog "<c:abandoned ship ai:25> .<d:500>.<d:500>.<d:500>.<d:500> "
                           "PROCESSING INTERRUPT... <B:0> BLOCK DETECTED MISSING! "
                           "<B:0> HOSTILE THREAT DETECTED")
                   (defn on-dialog-closed [0]
                     (map (curry room-new (opponent))
                          '((forcefield* 0 10)
                            (forcefield* 0 11)
                            (forcefield* 0 9)
                            (forcefield 1 11)
                            (forcefield 1 10)
                            (forcefield* 3 7)
                            (forcefield* 4 6)
                            (forcefield* 3 6)
                            (forcefield* 4 7)
                            (forcefield 6 5)
                            (forcefield* 8 5)
                            (forcefield* 8 9)
                            (forcefield* 9 9)
                            (forcefield 9 7)
                            (forcefield 10 7)))
                     (dialog "The vessel begins charging its weapons...")
                     (setq on-dialog-closed nil)))))

            (setq opts (filter (lambda (not (equal $0 wpn))) opts))
            (dialog-opts-reset)
            (dialog-opts-push (string "take " (rinfo 'name (get opts 0)) "…") wake)
            (dialog-opts-push (string "take " (rinfo 'name (get opts 1)) "…") wake)
            (unbind 'take))))))))