let () = Li

let fib n = 
  let rec helper n a b =
    if n<=2 then 1
    else helper (n-1) b (a+b)
  in

  let foo2 () =
    let rec helper n a b =
      if n<=2 then 1
      else helper (n-1) b (a+b)
    in
    ()
  in
  helper n


let cartesian xs ys = 
  List.concat @@ List.map (fun y -> List.map (fun x -> (x,y)) xs) ys

let () = ();;
