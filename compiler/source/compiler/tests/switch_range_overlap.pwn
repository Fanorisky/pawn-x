F(x)
{
    switch (x)
    {
        case 0..9: return 1;
        case 5:    return 2;   // overlaps the 0..9 range -> must still be caught
        default:   return 0;
    }
}
main() { return F(5); }
