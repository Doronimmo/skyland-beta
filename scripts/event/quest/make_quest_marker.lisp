

(let (;; c = corrupt nodes. A Node becomes corrupt when it falls into the storm.
      (c (filter
          (lambda (equal (car $0) 4))
          (wg-nodes)))
      (p (wg-pos)))

  ;; Sort corrupt nodes by x-value. We don't want to place a quest marker too
  ;; close to the storm frontier.
  (if c
      (setq c (sort c (lambda
                        (> (car (cdr $0)) (car (cdr $1)))))))

  (let ((n (filter
            (lambda
              (and
               (not (equal (car $0) 0))
               (not (equal (car $0) 4))
               (not (equal (car $0) 5))
               (not (equal $0 p))
               ;; The node must be somewhat ahead of the storm frontier; we
               ;; can't just place a node somewhere that's totally unreachable,
               ;; that's no fun.
               (> (car (cdr $0))
                  (if c
                      (+ 4 (car (cdr (car c))))
                    4))))
            (wg-nodes))))

    (if (> (length n) 4)
        (progn
          (setq n (sort
                   n
                   (lambda
                     ;; Sort by manhattan length from current coord,
                     ;; descending.
                     (> (+ (abs (- (car (cdr $0)) (car (cdr p))))
                           (abs (- (cdr (cdr $0)) (cdr (cdr p)))))
                        (+ (abs (- (car (cdr $1)) (car (cdr p))))
                           (abs (- (cdr (cdr $1)) (cdr (cdr p)))))))))
          (let ((n (get n (choice 3))))
            (wg-node-set
             (car (cdr n))
             (cdr (cdr n))
             10)
            n))
      nil)))