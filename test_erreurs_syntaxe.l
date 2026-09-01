int main()
    # Erreur : Nom de variable invalide
    123bad_var = 10

    # Erreur : Syntaxe match mal formée
    match with
        | 1 -> print("Erreur")

    # Erreur : Dépassement d'indentation (si imbrications excessives)
    if true:
        if true:
            if true:
                print("test")
