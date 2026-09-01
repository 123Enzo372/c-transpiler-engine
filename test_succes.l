#all
#linux

int calculate()
    return 42

int main()
    # Inférence de type : entiers, flottants, expressions et retours de fonction
    a = 10
    pi = 3.14
    ratio = a + 2.5
    res = calculate()
    
    # Listes et interpolation de chaines
    numbers = [1, 2, 3]
    print("Inférence : a={a}, ratio={ratio}, res={res}")

    # Bloc de processus et pipe anonyme
    #pipe
    #process
    #enfant
        valeur = 100
        print("Enfant écrit : {valeur}")
        return 0
    #parent basic
        print("Parent synchrone terminé")

    # Match étendu avec gardes conditionnelles
    n = 5
    match n with
    | _ if n > 10 -> print("Grand nombre")
    | _ if n > 0 -> print("Nombre positif")
    | _ -> print("Autre")

    items = [10, 20, 30]
    match items with
        | [] -> print("Liste vide")
        | [x] -> print("Un seul element : {x}")
        | head::tail -> print("Plusieurs elements")

    return 0
