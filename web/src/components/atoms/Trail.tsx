import { Breadcrumbs, Link } from "@mui/material";
import { useNavigate } from "react-router";
import Typography from "@mui/material/Typography";

interface TrailItem {
  title: string;
  link: string;
}

interface Props {
  title?: string;
  trail?: TrailItem[];
}

export default function Trail({ trail = [], title = "" }: Props) {
  const navigate = useNavigate();
  return (
    <Breadcrumbs>
      {trail.map(({ title, link }) => (
        <Link underline="hover" color="inherit" key={link} onClick={() => navigate(link)} href="#">
          {title}
        </Link>
      ))}
      <Typography sx={{ color: "text.primary" }}>{title}</Typography>
    </Breadcrumbs>
  );
}
